# hispark_sr
Convert a super-resolution model(ournet) from pytorch to wk for Hisilicon Hi3516DV300.  
The Python Codes include the SuperResolution model train codes and test codes.  
The C Codes include the codes for deploying the model on taurus.  
#### Prepare
inference_realesrgan.py is running ournet model on python(Desktop).  

#### Convert Model
I first convert ournet model from pytorch to onnx. 
Then on the Ubuntu, I use onnx2caffe to convert onnx model to caffe model.  
On RuyiStudio, I convert the caffe model to the wk model for NPU.  
Because Hi3516DV300 NPU cannot support PixelShuffle, I remove PixelShuffle in pytorch and convert the model to caffe model.  
Therefore the PixelShuffle module is realized on CPU of Hi3516DV300. This module occupies much process time. Recently, I have made a lot of optimizations with NEON assembly.  

#### Deployment
In ohos_ive_demo, it is about sending orginal images(left) and super-resolution(right) images to HDMI.  
In ohos_helloworld_demo, it is about displaying super-resolution images on LCD.

##### Made by Wang Yuhang, Dong Zhetao and Xin Zhe
