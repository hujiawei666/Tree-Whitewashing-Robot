import os
import ujson
import aicube
from libs.PipeLine import ScopedTiming
from libs.Utils import *
from media.sensor import *
from media.display import *
from media.media import *
from machine import UART
from machine import FPIOA
import nncase_runtime as nn
import ulab.numpy as np
import image
import gc
import time

# 串口配置
fpioa = FPIOA()
fpioa.set_function(11, FPIOA.UART2_TXD)
fpioa.set_function(12, FPIOA.UART2_RXD)

# 初始化UART2，波特率115200，8位数据位，无校验，1位停止位
uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, 
            parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE,
            timeout=100)  # 增加100ms超时设置

display_mode = "lcd"
if display_mode == "lcd":
    DISPLAY_WIDTH = ALIGN_UP(800, 16)  # 屏幕宽度（LCD模式，例如800像素）
    DISPLAY_HEIGHT = 480
else:
    DISPLAY_WIDTH = ALIGN_UP(1920, 16)  # 屏幕宽度（其他模式，例如1920像素）
    DISPLAY_HEIGHT = 1080

# 计算屏幕X方向中心点坐标
SCREEN_CENTER_X = DISPLAY_WIDTH // 2  # 例如800//2=400，1920//2=960

OUT_RGB888P_WIDTH = ALIGN_UP(1280, 16)
OUT_RGB888P_HEIGH = 720

root_path = "/sdcard/mp_deployment_source/"
config_path = root_path + "deploy_config.json"
deploy_conf = {}
debug_mode = 1

def two_side_pad_param(input_size, output_size):
    ratio_w = output_size[0] / input_size[0]
    ratio_h = output_size[1] / input_size[1]
    ratio = min(ratio_w, ratio_h)
    new_w = int(ratio * input_size[0])
    new_h = int(ratio * input_size[1])
    dw = (output_size[0] - new_w) / 2
    dh = (output_size[1] - new_h) / 2
    top = int(round(dh - 0.1))
    bottom = int(round(dh + 0.1))
    left = int(round(dw - 0.1))
    right = int(round(dw - 0.1))
    return top, bottom, left, right, ratio

def read_deploy_config(config_path):
    with open(config_path, 'r') as json_file:
        try:
            config = ujson.load(json_file)
        except ValueError as e:
            print("JSON 解析错误:", e)
    return config

def detection():
    print("det_infer start")
    deploy_conf = read_deploy_config(config_path)
    kmodel_name = deploy_conf["kmodel_path"]
    labels = deploy_conf["categories"]
    confidence_threshold = deploy_conf["confidence_threshold"]
    nms_threshold = deploy_conf["nms_threshold"]
    img_size = deploy_conf["img_size"]
    num_classes = deploy_conf["num_classes"]
    color_four = get_colors(num_classes)
    nms_option = deploy_conf["nms_option"]
    model_type = deploy_conf["model_type"]
    if model_type == "AnchorBaseDet":
        anchors = deploy_conf["anchors"][0] + deploy_conf["anchors"][1] + deploy_conf["anchors"][2]
    kmodel_frame_size = img_size
    frame_size = [OUT_RGB888P_WIDTH, OUT_RGB888P_HEIGH]
    strides = [8, 16, 32]

    top, bottom, left, right, ratio = two_side_pad_param(frame_size, kmodel_frame_size)

    # 初始化kpu和ai2d
    kpu = nn.kpu()
    kpu.load_kmodel(root_path + kmodel_name)
    ai2d = nn.ai2d()
    ai2d.set_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)
    ai2d.set_pad_param(True, [0, 0, 0, 0, top, bottom, left, right], 0, [114, 114, 114])
    ai2d.set_resize_param(True, nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
    ai2d_builder = ai2d.build([1, 3, OUT_RGB888P_HEIGH, OUT_RGB888P_WIDTH], 
                             [1, 3, kmodel_frame_size[1], kmodel_frame_size[0]])

    # 初始化传感器和显示
    sensor = Sensor()
    sensor.reset()
    sensor.set_hmirror(False)
    sensor.set_vflip(False)
    sensor.set_framesize(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    sensor.set_pixformat(PIXEL_FORMAT_YUV_SEMIPLANAR_420)
    sensor.set_framesize(width=OUT_RGB888P_WIDTH, height=OUT_RGB888P_HEIGH, chn=CAM_CHN_ID_2)
    sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_2)
    sensor_bind_info = sensor.bind_info(x=0, y=0, chn=CAM_CHN_ID_0)
    Display.bind_layer(** sensor_bind_info, layer=Display.LAYER_VIDEO1)
    if display_mode == "lcd":
        Display.init(Display.ST7701, to_ide=True)
    else:
        Display.init(Display.LT9611, to_ide=True)
    osd_img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
    MediaManager.init()
    sensor.run()

    rgb888p_img = None
    ai2d_input_tensor = None
    data = np.ones((1, 3, kmodel_frame_size[1], kmodel_frame_size[0]), dtype=np.uint8)
    ai2d_output_tensor = nn.from_numpy(data)
    
    # 中心线颜色（绿色）
    CENTER_LINE_COLOR = (0, 255, 0)
    
    # 添加全局变量用于计数
    no_target_count = 0
    SEND_THRESHOLD = 3  # 每3次检测失败才发送一次9999
    
    while True:
        with ScopedTiming("total", debug_mode > 0):
            rgb888p_img = sensor.snapshot(chn=CAM_CHN_ID_2)
            if rgb888p_img.format() == image.RGBP888:
                # 图像预处理与模型推理
                ai2d_input = rgb888p_img.to_numpy_ref()
                ai2d_input_tensor = nn.from_numpy(ai2d_input)
                ai2d_builder.run(ai2d_input_tensor, ai2d_output_tensor)
                kpu.set_input_tensor(0, ai2d_output_tensor)
                kpu.run()

                # 处理模型输出
                results = []
                for i in range(kpu.outputs_size()):
                    out_data = kpu.get_output_tensor(i)
                    result = out_data.to_numpy().reshape((-1,))  # 简化reshape
                    del out_data
                    results.append(result)

                # 后处理获取检测框
                det_boxes = aicube.anchorbasedet_post_process(
                    results[0], results[1], results[2], kmodel_frame_size, 
                    frame_size, strides, num_classes, confidence_threshold, 
                    nms_threshold, anchors, nms_option)
                
                # 计算识别框中心与屏幕中心的差值
                osd_img.clear()
                
                # 绘制中心参考线（绿色竖线）
                osd_img.draw_line(SCREEN_CENTER_X, 0, SCREEN_CENTER_X, DISPLAY_HEIGHT, color=CENTER_LINE_COLOR)
                
                if det_boxes:
                    # 选择置信度最高的目标
                    main_target = max(det_boxes, key=lambda x: x[1])
                    x1, y1, x2, y2 = main_target[2], main_target[3], main_target[4], main_target[5]
                    
                    # 转换识别框坐标到屏幕坐标系（使用浮点计算确保精度）
                    x1_screen = int(x1 * DISPLAY_WIDTH / OUT_RGB888P_WIDTH)
                    x2_screen = int(x2 * DISPLAY_WIDTH / OUT_RGB888P_WIDTH)
                    y_screen = int(y1 * DISPLAY_HEIGHT / OUT_RGB888P_HEIGH)
                    h_screen = int((y2 - y1) * DISPLAY_HEIGHT / OUT_RGB888P_HEIGH)
                    
                    # 计算识别框中心X坐标
                    box_center_x = (x1_screen + x2_screen) // 2
                    
                    # 计算与屏幕中心的差值（核心逻辑）
                    center_diff = box_center_x - SCREEN_CENTER_X  # 负值=左偏，正值=右偏
                    
                    # 绘制识别框
                    box_color = color_four[main_target[0]][1:]  # 获取类别对应颜色
                    osd_img.draw_rectangle(x1_screen, y_screen, 
                                          x2_screen - x1_screen, h_screen,
                                          color=box_color)
                    
                    # 绘制标签
                    text = f"{labels[main_target[0]]} {round(main_target[1], 2)}"
                    osd_img.draw_string_advanced(x1_screen, max(y_screen - 40, 10), 
                                                32, text, color=box_color)
                    
                    # 绘制方向指示器
                    if center_diff < 0:
                        direction = "←"  # 目标在中心左侧
                        direction_text = f"← {-center_diff}px"
                    elif center_diff > 0:
                        direction = "→"  # 目标在中心右侧
                        direction_text = f"→ {center_diff}px"
                    else:
                        direction = "◎"  # 目标在中心
                        direction_text = "◎ CENTER"
                    
                    # 在目标框上方绘制方向指示
                    osd_img.draw_string(box_center_x - 50, max(y_screen - 80, 5), 
                                       direction_text, color=(255, 0, 0), scale=2)
                    
                    # 串口发送差值（整数形式）
                    uart.write(f"{center_diff}\r\n")
                    print(f"Sent center diff: {center_diff}")
                    
                    # 重置未检测计数器
                    no_target_count = 0
                else:
                    # 增加未检测计数器
                    no_target_count += 1
                    
                    # 每3次检测失败才发送一次9999
                    if no_target_count >= SEND_THRESHOLD:
                        uart.write("9999\r\n")
                        print("Sent no target (9999)")
                        no_target_count = 0  # 发送后重置计数器
                    else:
                        print(f"No target detected, count: {no_target_count} (skip sending)")
                
                Display.show_image(osd_img, 0, 0, Display.LAYER_OSD3)
                gc.collect()
            
            # 释放资源并控制处理频率
            rgb888p_img = None
            time.sleep(0.1)  # 控制发送频率（10Hz）

    # 资源清理（实际运行中循环不会退出，此处为规范）
    del ai2d_input_tensor, ai2d_output_tensor
    sensor.stop()
    Display.deinit()
    MediaManager.deinit()
    gc.collect()
    time.sleep(1)
    nn.shrink_memory_pool()
    print("det_infer end")
    return 0

if __name__ == "__main__":
    detection()