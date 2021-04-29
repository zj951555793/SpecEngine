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

#include "SpecEngine.h"
#include "Utils.h"
#include <QMutex>
#include <QDebug>
#include <QtMath>
#include <QCoreApplication>
#include <QFile>
#include <QMetaObject>

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

const qint64 BufferDurationUs       = 5 * 1000000;

// Size of the level calculation window in microseconds
const int    LevelWindowUs          = 0.05 * 1000000;


SpecEngine::SpecEngine(QObject *parent)
    :   QObject(parent)
    ,   m_bufferLength()
    ,   m_dataLength()
    ,   m_levelBufferLength(0)
    ,   m_threshold(0.85)
    ,   m_decayRate(0.08)
    ,   m_smoothRadius(7)
    ,   m_rmsLevel(0.0)
    ,   m_peakLevel(0.0)
    ,   m_spectrumBufferLength(0)
    ,   m_spectrumAnalyser()

{
    qRegisterMetaType<FrequencySpectrum>("FrequencySpectrum");
    qRegisterMetaType<WindowFunction>("WindowFunction");

    connect(&m_spectrumAnalyser, QOverload<const FrequencySpectrum&>::of(&SpectrumAnalyser::spectrumChanged),
            this, QOverload<const FrequencySpectrum&>::of(&SpecEngine::slotspectrumChanged));

    timer.setInterval(17);
    connect(&timer,&QTimer::timeout,this,&SpecEngine::audioNotify);
    connect(&m_capture,&AudioCapture::notifyData,this,&SpecEngine::onCatpureData);
    connect(&m_capture,&AudioCapture::formatChanged,this,&SpecEngine::onFormatChanged);

    for(int i=0;i<10;i++)       //平滑范围为10
        m_pulseBuffer.push_back(0);
    m_pulseSum=0.0;

    for(int i=0;i<5;i++)       //平滑范围为10
        m_pulseBuffer2.push_back(0);
    m_pulseSum2=0.0;
}

SpecEngine *SpecEngine::getEngine()
{
    static SpecEngine engine;
    return &engine;
}

SpecEngine::~SpecEngine()
{

}

//-----------------------------------------------------------------------------
// Public functions
//-----------------------------------------------------------------------------



qint64 SpecEngine::bufferLength() const
{
    return m_bufferLength;
}

void SpecEngine::setWindowFunction(WindowFunction type)
{
    m_spectrumAnalyser.setWindowFunction(type);
}

void SpecEngine::audioNotify()
{
    const qint64 levelPosition = m_dataLength - m_levelBufferLength;
    if (levelPosition >= 0){
        calculateLevel(levelPosition, m_levelBufferLength);
    }
    if (m_dataLength >= m_spectrumBufferLength) {
        const qint64 spectrumPosition = m_dataLength - m_spectrumBufferLength;
        calculateSpectrum(spectrumPosition);
    }
}

void SpecEngine::calculateLevel(qint64 position, qint64 length)
{
    Q_ASSERT(position + length <=  m_dataLength);
    qreal peakLevel = 0.0;
    qreal sum = 0.0;
    const char *ptr = m_buffer.constData() + position ;
    const char *const end = ptr + length;
    qreal fracValue ;
    while (ptr < end) {
        const qint16 value = *reinterpret_cast<const qint16*>(ptr);
        fracValue = pcmToReal(value);
        peakLevel = qMax(peakLevel, fracValue);
        sum += fracValue * fracValue;
        ptr += 2;
    }
    const int numSamples = length / 2;
    qreal rmsLevel = qBound(0.0,sqrt(sum / numSamples),1.0);
    setLevel(rmsLevel, peakLevel, numSamples);
}

void SpecEngine::slotspectrumChanged(const FrequencySpectrum &spectrum)
{
    m_spectrum=spectrum;
    QVector<double> sorted;
    for(int i=0;i<spectrum.count();i++){
        if(spectrum[i].amplitude>0.05)
            sorted.push_back(spectrum[i].amplitude);
    }
    qSort(sorted);
    double level=sorted.empty()?0:sorted[(sorted.size()-1)*m_threshold];
    last_data.resize(spectrum.count());
    for(int i=0;i<m_spectrum.count();i++){
        m_spectrum[i].amplitude=qMax(0.0,(m_spectrum[i].amplitude-level)/(1.0-level));

        m_spectrum[i].amplitude=last_data[i]*m_decayRate+m_spectrum[i].amplitude*(1.0-m_decayRate);

        last_data[i]=m_spectrum[i].amplitude;
    }
    smooth(m_spectrum,m_smoothRadius);
//    smooth(m_spectrum,createMat(m_smoothRadius,10));
    updateBars();
}

void SpecEngine::calculateSpectrum(qint64 position)
{
    Q_ASSERT(position + m_spectrumBufferLength <=  m_dataLength);
    Q_ASSERT(0 == m_spectrumBufferLength % 2);        // 傅里叶运算要求数据大小是2的指数倍
    if (m_spectrumAnalyser.isReady()) {
        m_spectrumBuffer = QByteArray::fromRawData(m_buffer.constData() + position , m_spectrumBufferLength);
        m_spectrumAnalyser.calculate(m_spectrumBuffer, m_format);
    }
}

void SpecEngine::calculatePulse(const QByteArray &data)
{
    const int bytesPerSample =  m_format.wBitsPerSample / 8;                   //每一帧的字节长度（单通道）
    const char *ptr = data.constData();
    const char *end = ptr+data.size();
    qreal sum=0.0;
    while (ptr<end) {
        for(int i=0;i<m_format.nChannels;i++){
            const qint16 pcmSample = *reinterpret_cast<const qint16*>(ptr);     //依次读取各个通道的数据
            const qreal realSample = pcmToReal(pcmSample);
            sum+=realSample*realSample;
            ptr+=bytesPerSample;
        }
    }
    qreal pulse=sqrt(sum/data.size()*bytesPerSample);
    m_pulseBuffer.push_back(pulse);
    m_pulseSum=m_pulseSum+m_pulseBuffer.back()-m_pulseBuffer.front();
    m_pulseBuffer.pop_front();
    m_pulseBuffer2.push_back(qMax(0.,pulse-m_pulseSum/m_pulseBuffer.size())/(1.0-pulse));
    m_pulseSum2=m_pulseSum2+m_pulseBuffer2.back()-m_pulseBuffer2.front();
    m_pulseBuffer2.pop_front();
    m_pulse=m_pulseSum2/m_pulseBuffer2.size();
}

void SpecEngine::setLevel(qreal rmsLevel, qreal peakLevel, int)
{
    m_rmsLevel = m_rmsLevel*m_decayRate+rmsLevel*(1.0-m_decayRate);
    m_peakLevel = m_peakLevel*m_decayRate+peakLevel*(1.0-m_decayRate);
}

void SpecEngine::start()
{
    m_capture.startCapture();
    timer.start();
}

void SpecEngine::stop()
{
    m_capture.stopCapture();
}

qint64 audioLength(WAVEFORMATEX wav, qint64 microSeconds)
{
   qint64 result = (wav.nSamplesPerSec * wav.nChannels * (wav.wBitsPerSample / 8))
           *  microSeconds/ 1000000;
   result -= result % (wav.nChannels * wav.wBitsPerSample);
   return result;
}

void SpecEngine::onFormatChanged(WAVEFORMATEX wav)
{
    m_format=wav;
    m_spectrumBufferLength = SpectrumLengthSamples *
                            (wav.wBitsPerSample / 8) * wav.nChannels;
    m_bufferLength=audioLength(wav,BufferDurationUs);
    m_levelBufferLength=audioLength(wav,LevelWindowUs);
    m_buffer.resize(m_bufferLength);
    m_buffer.fill(0);
}

void SpecEngine::setInterval(int msc)
{
    timer.setInterval(msc);
}

bool SpecEngine::isRunning() const
{
    return m_capture.isRunning();
}

void SpecEngine::onCatpureData(const QByteArray &data)
{
    if(m_dataLength+m_spectrumBufferLength>=m_bufferLength){
        memcpy_s(m_buffer.data(), m_spectrumBufferLength, m_buffer.data()+m_dataLength-m_spectrumBufferLength, m_spectrumBufferLength);
        m_dataLength=m_spectrumBufferLength;
    }
    memcpy_s(m_buffer.data()+m_dataLength, data.size(), data.data(), data.size());
    m_dataLength+=data.size();
    calculatePulse(data);
}

void SpecEngine::registerSpec(Spectrum* spec)
{
    if(spec->data.empty())
        return;
    specs.push_back(spec);
}

void SpecEngine::removeSpec(Spectrum *index)
{
    specs.removeOne(index);
}

void SpecEngine::smooth(FrequencySpectrum &spectrum, const int &windowSize)
{
    if(spectrum.count()==0||windowSize==0)
       return;
    QVector<qreal> window(windowSize);
    qreal sum=spectrum[0].amplitude*window.size();
    for(int i=0;i<windowSize;i++){
        window[i]=m_spectrum[i].amplitude;
        sum+=window[i];
    }
    int rWidth=windowSize/2;
    int i=rWidth;

    while (i < m_spectrum.count()-rWidth)
    {
        m_spectrum[i].amplitude=sum/ window.size();
        window.push_back(m_spectrum[i+rWidth].amplitude);
        sum=sum-window.front()+window.back();
        window.pop_front();
        i++;
    }
}

//Eigen::MatrixXd SpecEngine::createMat(int m, int k)
//{
//    Eigen::MatrixXd X;
//    X.resize(2*m+1,k);
//    for(int i=0;i<2*m+1;i++){
//        for(int j=0;j<k;j++){
//            X(i,j)=pow(i-m,j);
//        }
//    }
//    return X*(X.transpose()*X).inverse()*X.transpose();
//}

//void SpecEngine::smooth(FrequencySpectrum &spectrum, Eigen::MatrixXd mat)
//{
//    int m=(mat.rows()-1)/2;
//    for(int i=m;i<spectrum.count()-m;i++){
//        for(int j=0;j<mat.rows();j++){
//            double sum=0.0;
//            for(int k=0;k<mat.cols();k++){
//                sum+=mat(j,k)*spectrum[i+k-m].amplitude;
//            }
//            spectrum[i+j-m].amplitude=sum;
//        }
//    }
//}

int SpecEngine::barIndex(const qreal &frequency, const Spectrum &spec)
{
    Q_ASSERT(frequency > spec.lowFreq && frequency < spec.highFreq);
    const qreal bandWidth = (spec.highFreq - spec.lowFreq) / spec.data.size();
    const int index =(frequency - spec.lowFreq) / bandWidth;
    return qBound(0,index,spec.data.size()-1);
}

void SpecEngine::updateBars()
{

    FrequencySpectrum::const_iterator i = m_spectrum.begin();
    const FrequencySpectrum::const_iterator end = m_spectrum.end();
    for(auto spec=specs.begin();spec!=specs.end();++spec){
        for(int i=0;i<(*spec)->data.size();i++)
           (*spec)->data[i]=0;
    }
    for ( ; i != end; ++i) {
       const FrequencySpectrum::Element e = *i;
       for(auto spec=specs.begin();spec!=specs.end();++spec){
           if(e.frequency>(*spec)->lowFreq&&e.frequency<(*spec)->highFreq){
              float& value=(*spec)->data[barIndex(e.frequency,(**spec))];
              value=qMax(value,(float)e.amplitude);
           }
       }
    }
}

qreal SpecEngine::getPulse() const
{
    return m_pulse;
}

qreal SpecEngine::getPeakLevel() const
{
    return m_peakLevel;
}

qreal SpecEngine::getRmsLevel() const
{
    return m_rmsLevel;
}

//int SpecEngine::getSmoothDegree() const
//{
//    return m_smoothDegree;
//}

//void SpecEngine::setSmoothDegree(int smoothDegree)
//{
//    m_smoothDegree = smoothDegree;
//}

int SpecEngine::getSmoothRadius() const
{
    return m_smoothRadius;
}

void SpecEngine::setSmoothRadius(int smoothRadius)
{
    m_smoothRadius = smoothRadius;
}

QList<QString> SpecEngine::getDevices() const
{
    QList<QString> list;
    for(const auto&it:m_capture.getDevices()){
        list<<it.name;
    }
    return list;
}

void SpecEngine::setDevice(const QString &name)
{
    m_capture.setCurrentDevice(name);
}

qreal SpecEngine::getSmoothFactor() const
{
    return m_decayRate;
}

void SpecEngine::setSmoothFactor(const qreal &decayRate)
{
    Q_ASSERT(decayRate>=0&&decayRate<=1.0);
    m_decayRate = decayRate;
}

qreal SpecEngine::getThreshold() const
{
    return m_threshold;
}

void SpecEngine::setThreshold(const qreal &threshold)
{
    Q_ASSERT(threshold>=0&&threshold<=1.0);
    m_threshold = threshold;
}

Spectrum::Spectrum(int lowFreq, int highFreq, int samples)
    :lowFreq(lowFreq)
    ,highFreq(highFreq)
    ,data(samples)
{
    SpecEngine::getEngine()->registerSpec(this);
}

Spectrum::~Spectrum()
{
    SpecEngine::getEngine()->removeSpec(this);
}
