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
  ???????????????????????????????????????Spectrum?????????????????????data??????????????????????????????[0,1]
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
     * ????????????
     */
    static SpecEngine *getEngine();
    ~SpecEngine();

    /**
     * ????????????????????????
     */
    void start();

    /**
     * ??????????????????
     */
    void stop();

    /**
     * ???????????????????????????
     */
    bool isRunning() const;

    /**
     * ????????????????????????????????????17ms
     */
    void setInterval(int msc);

    /**
     * ?????????????????????????????????[0,1]
     */
    qreal getSmoothFactor() const;

    /**
     * ????????????????????????????????????[0,1],?????????????????????????????????????????????
     */
    void setSmoothFactor(const qreal &decayRate);

    /**
     * ?????????????????????????????????[0,1]
     */
    qreal getThreshold() const;
    /**
     * ???????????????????????????????????????[0,1]????????????0.85?????????85%??????????????????
     */
    void setThreshold(const qreal &threshold);

    /**
     * ??????????????????
     */
    int getSmoothRadius() const;

    /**
     * ???????????????????????????????????????????????????????????????
     */
    void setSmoothRadius(int smoothRadius);

    /**
      ????????????????????????????????????????????????????????????
    */
    void setWindowFunction(WindowFunction type);

    /**
      ????????????????????????
    */

    QList<QString> getDevices() const;

    void setDevice(const QString& name);

    /**
      ??????????????????????????????????????????
    */
    qreal getRmsLevel() const;
    /**
      ??????????????????????????????????????????
    */

    qreal getPeakLevel() const;
    /**
      ?????????????????????????????????????????????
    */
    qreal getPulse() const; 

signals:
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);
    void spectrumChanged(const FrequencySpectrum &spectrum);

private :
//  ????????????
    void registerSpec(Spectrum* index);

    void removeSpec(Spectrum* index);

//  ??????????????????
    void audioNotify();

    void slotspectrumChanged(const FrequencySpectrum &spectrum);

    //  ??????????????????,?????????????????????????????????????????????
private:
    explicit SpecEngine(QObject *parent = 0);

//  ????????????????????????????????????
    qint64 bufferLength() const;

//  ????????????????????????????????????
    qint64 dataLength() const { return m_dataLength; }

//  ????????????
    void calculateLevel(qint64 position, qint64 length);

//  ????????????
    void calculateSpectrum(qint64 position);

//  ????????????????????????
    void calculatePulse(const QByteArray& data);

//  ??????????????????
    void smooth(FrequencySpectrum &spectrum, const int &);

////  S-G????????????
//    Eigen::MatrixXd createMat(int m,int k);
//    void smooth(FrequencySpectrum &spectrum, Eigen::MatrixXd mat);

    void setLevel(qreal rmsLevel, qreal peakLevel, int numSamples);

//  ?????????????????????????????????
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
    WAVEFORMATEX        m_format;       //????????????
    QByteArray          m_buffer;       //?????????
    qint64              m_bufferLength; //???????????????
    qint64              m_dataLength;   //????????????
    int                 m_levelBufferLength;
    qreal               m_threshold;    //????????????
    qreal               m_decayRate;    //????????????
    int                 m_smoothRadius; //????????????
//   int                 m_smoothDegree;  //??????????????????
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
