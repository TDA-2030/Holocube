# coding:utf-8 

import socket

import cv2

def num2uint32(num):
    l=[num&0xff, (num>>8)&0xff, (num>>16)&0xff, (num>>24)&0xff]
    return l

def num2uint64(num):
    l=[num&0xff, (num>>8)&0xff, (num>>16)&0xff, (num>>24)&0xff, (num>>32)&0xff, (num>>40)&0xff, (num>>48)&0xff, (num>>56)&0xff]
    return l

if __name__ == "__main__":
    
    # a=num2uint32(36)
    # b=num2uint32(10240)
    # print(a)
    # print(b)
    # print(a+b)
    # ba=bytearray([36, 1, 0, 0, 0, 40, 0, 0])
    # print(ba)
    # print(len(ba))
    # exit(0)
    print('请保障PC和开发板访问同一个局域网！')
    ip = "192.168.50.86"#input('请输入开发板IP地址：')
    cap = cv2.VideoCapture('video.mp4')
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((ip, 8081))
    encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), 90]
    while cap.isOpened():
        _, frame = cap.read()
        dst = cv2.resize(frame, (160, 80))
        _, jpg = cv2.imencode('.jpg', dst, encode_param)
        ba = bytearray(jpg)
        cv2.imshow('frame', frame)
        head=num2uint32(36) + num2uint32(len(ba)) + num2uint64(0)
        print(head)
        s.send(bytearray(head))
        s.send(ba)
        if cv2.waitKey(25) & 0xFF == ord('q'):
            break
    cap.release()
    cv2.destroyAllWindows()
    s.close()
