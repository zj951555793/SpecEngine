/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SPEC_ENGINE_H
#define SPEC_ENGINE_H


#include "AudioCapture.h"
#include <QTimer>
#include "SpectrumAnalyser.h"
//#include "Eigen"
class FrequencySpectrum;
/**
  如需使用频谱，只需创建一个Spectrum即可，通过读取data中的数据能得到频谱值[0,1]
*/
struct  Spectrum{
    int lowFreq;
    int highFreq;
    QVector<float> data;
    Spectrum(int lowFreq,int highFreq,int samples);
    ~Spectrum();
};

class SpecEngine : public QObject
{
    Q_OBJECT
public:
    friend Spectrum;

    /**
     * 静态单例
     */
    static SpecEngine *getEngine();
    ~SpecEngine();

    /**
     * 开启频谱分析引擎
     */
    void start();

    /**
     * 停止频谱分析
     */
    void stop();

    /**
     * 判断引擎是否在运行
     */
    bool isRunning() const;

    /**
     * 设置响应时间间隔，默认为17ms
     */
    void setInterval(int msc);

    /**
     * 获取平滑因子，值区间为[0,1]
     */
    qreal getSmoothFactor() const;

    /**
     * 设置平滑因子，取值区间为[0,1],平滑因子越高，频谱的运动越缓慢
     */
    void setSmoothFactor(const qreal &decayRate);

    /**
     * 获取过滤百分比，区间为[0,1]
     */
    qreal getThreshold() const;
    /**
     * 设置过滤百分比，取值区间为[0,1]，假如为0.85，则有85%的数据被过滤
     */
    void setThreshold(const qreal &threshold);

    /**
     * 获取平滑范围
     */
    int getSmoothRadius() const;

    /**
     * 设置平滑范围，平滑范围越大，音频数据越平滑
     */
    void setSmoothRadius(int smoothRadius);

    /**
      设置频谱分析的滤波窗函数，默认为高斯滤波
    */
    void setWindowFunction(WindowFunction type);

    /**
      获取输入设备列表
    */

    QList<QString> getDevices() const;

    void setDevice(const QString& name);

    /**
      获取一定时间内音频节奏的均值
    */
    qreal getRmsLevel() const;
    /**
      获取一定时间内音频节奏的峰值
    */

    qreal getPeakLevel() const;
    /**
      获取一定时间内音频节奏的脉冲值
    */
    qreal getPulse() const; 

signals:
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);
    void spectrumChanged(const FrequencySpectrum &spectrum);

private :
//  注册频谱
    void registerSpec(Spectrum* index);

    void removeSpec(Spectrum* index);

//  引擎响应函数
    void audioNotify();

    void slotspectrumChanged(const FrequencySpectrum &spectrum);

    //  设置音频格式,默认使用默认输出设备的音频格式
private:
    explicit SpecEngine(QObject *parent = 0);

//  内部引擎缓冲区的字节长度
    qint64 bufferLength() const;

//  返回已填充数据的字节长度
    qint64 dataLength() const { return m_dataLength; }

//  等级分析
    void calculateLevel(qint64 position, qint64 length);

//  频谱分析
    void calculateSpectrum(qint64 position);

//  节奏（脉冲）分析
    void calculatePulse(const QByteArray& data);

//  滑动窗口滤波
    void smooth(FrequencySpectrum &spectrum, const int &);

////  S-G平滑滤波
//    Eigen::MatrixXd createMat(int m,int k);
//    void smooth(FrequencySpectrum &spectrum, Eigen::MatrixXd mat);

    void setLevel(qreal rmsLevel, qreal peakLevel, int numSamples);

//  处理实时采集的音频数据
    void onCatpureData(const QByteArray&data);
    void onFormatChanged(WAVEFORMATEX wav);
private:
    QVector<double> last_data;
    QList<Spectrum*> specs;

private slots:
    int barIndex(const qreal &frequency, const Spectrum& spec);
    void updateBars();
private:
    QTimer timer;
    FrequencySpectrum   m_spectrum;
    WAVEFORMATEX        m_format;       //音频格式
    QByteArray          m_buffer;       //缓冲区
    qint64              m_bufferLength; //缓冲区大小
    qint64              m_dataLength;   //数据大小
    int                 m_levelBufferLength;
    qreal               m_threshold;    //频谱阈值
    qreal               m_decayRate;    //衰减速度
    int                 m_smoothRadius; //平滑半径
//   int                 m_smoothDegree;  //平滑曲线阶数
    qreal               m_rmsLevel;
    qreal               m_peakLevel;

    qreal               m_pulse;
    QList<qreal>        m_pulseBuffer;
    QList<qreal>        m_pulseBuffer2;
    qreal               m_pulseSum;
    qreal               m_pulseSum2;

    int                 m_spectrumBufferLength;
    QByteArray          m_spectrumBuffer;
    AudioCapture        m_capture;
    SpectrumAnalyser    m_spectrumAnalyser;
};

#endif // ENGINE_H
