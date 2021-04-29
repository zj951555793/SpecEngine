# SpecEngine——Musical Visualization Engine
编译环境：

* Qt 5.14.2
* 编译器版本 MinGW 730 32位

## SpecEngine做了什么？

使用WASAPI采集Windows音频输出设备的音频数据，通过傅里叶运算得到频谱数据，这些数据可用于实现音乐可视化效果。

## 如何使用SpecEngine？

* **开启/关闭引擎**
```C++
SpecEngine::getEngine->start();
SpecEngine::getEngine->stop();
```
* **获取频谱数据**
只需创建Spectrum对象，直接使用就行（可动态修改频率范围和采样数）
```C++
Spectrum spectrum(-40000,40000,100);    //创建[-40000HZ,40000HZ]且采样数为100的频谱采样器
                                        //其中负值表示左声道，正值为右声道
qDebug()<<spectrum.data；               //打印频谱数据，结构为QVector<float>，尺寸为采样数（100）
```

* **获取单个数值的节奏数据**
    能够得到一个[0,1]之前的浮点值，它是描述一定时间内音频“节奏”的变化情况。
 *  节奏均值  
 ```SpecEngine::getEngine->getRmsLevel()```  
 *  节奏峰值  
 ```SpecEngine::getEngine->getPeakLevel()```
 *  节奏脉冲：有很好的节奏响应效果  
 ```SpecEngine::getEngine->getPulse()```

* **引擎设置**
 * 设置频谱分析时间间隔（毫秒数）  
   ```void setInterval(int msc)```  
 * 设置过滤阈值，取值范围为[0.0，1.0]，假如为0.85，则有85%的频谱数据被忽略掉  
   ```void setThreshold(const qreal &threshold)```  
 * 设置平滑因子，取值区间为[0,1],平滑因子越高，频谱的运动越缓慢  
  ```void setSmoothFactor(const qreal &decayRate)```
 * 设置平滑范围，平滑范围越大，音频数据越平滑  
  ```void setSmoothRadius(int smoothRadius)```
 * 设置频谱分析的滤波窗函数，默认为高斯滤波  
  ```void setWindowFunction(WindowFunction type)```
    
## 关于SpecEngine
SpecEngine 实现思路参考Qt官方Example——Spectrum，Italink(我)更换了音频采集模块，对频谱数据进行平滑处理，重新封装API，断断续续修改和优化了一年多，实时频谱效果可以说已经非常接近AE渲染的效果了，很多朋友都对这个代码特别感兴趣，因此花了几天把这部分代码剔出来重新封装，喜欢的朋友点个Star哦!

## 关于Italink 
QQ：657959053  
WX：f657959053  
CSDN：https://blog.csdn.net/qq_40946921  
Bilibili：https://www.bilibili.com/read/cv10421402  

