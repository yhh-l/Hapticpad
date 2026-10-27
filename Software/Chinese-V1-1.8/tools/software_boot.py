import ctypes as C, pathlib,struct,signal
signal.alarm(20)
lib=C.CDLL(str(pathlib.Path(__file__).resolve().parent/'libusb-1.0.0.dylib'))
P=C.c_void_p; I=C.c_int
lib.libusb_init.argtypes=[C.POINTER(P)];lib.libusb_init.restype=I
lib.libusb_open_device_with_vid_pid.argtypes=[P,C.c_uint16,C.c_uint16];lib.libusb_open_device_with_vid_pid.restype=P
lib.libusb_control_transfer.argtypes=[P,C.c_uint8,C.c_uint8,C.c_uint16,C.c_uint16,P,C.c_uint16,C.c_uint];lib.libusb_control_transfer.restype=I
lib.libusb_get_string_descriptor_ascii.argtypes=[P,C.c_uint8,P,I];lib.libusb_get_string_descriptor_ascii.restype=I
lib.libusb_close.argtypes=[P];lib.libusb_exit.argtypes=[P]
ctx=P();assert lib.libusb_init(C.byref(ctx))==0
h=lib.libusb_open_device_with_vid_pid(ctx,0x239a,0xcafe)
if not h: raise RuntimeError('Cannot open Pico USB control endpoint')
try:
 serial=C.create_string_buffer(256);n=lib.libusb_get_string_descriptor_ascii(h,3,serial,256)
 assert n>0 and serial.value==b'E46468C09732292D',(n,serial.value)
 print('Target serial verified:',serial.value.decode(),flush=True)
 # Read the configuration descriptor; identify the actual CDC communication interface.
 config=C.create_string_buffer(4096)
 n=lib.libusb_control_transfer(h,0x80,6,0x0200,0,config,4096,1500)
 if n<9: raise RuntimeError(f'Cannot read USB configuration: {n}')
 raw=config.raw[:n];idx=0;cdc=None
 while idx+2<=len(raw):
  length=raw[idx]
  if length<2:break
  if raw[idx+1]==4 and length>=9 and raw[idx+5]==2:cdc=raw[idx+2];break
  idx+=length
 assert cdc is not None,'No CDC communication interface'
 print('CDC interface:',cdc,flush=True)
 coding=C.create_string_buffer(struct.pack('<IBBB',1200,0,0,8))
 rc=lib.libusb_control_transfer(h,0x21,0x20,0,cdc,coding,7,2000)
 print('SET_LINE_CODING 1200:',rc,flush=True)
 if rc!=7: raise RuntimeError('1200 baud request failed')
 rc=lib.libusb_control_transfer(h,0x21,0x22,1,cdc,None,0,2000)
 print('DTR high:',rc,flush=True)
 if rc<0:raise RuntimeError('DTR high failed')
 rc=lib.libusb_control_transfer(h,0x21,0x22,0,cdc,None,0,2000)
 print('DTR low / enter BOOT:',rc,flush=True)
finally:
 lib.libusb_close(h);lib.libusb_exit(ctx)
