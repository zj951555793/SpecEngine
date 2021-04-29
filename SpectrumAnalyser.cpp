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

#include "SpectrumAnalyser.h"
#include "Utils.h"
#include "fftreal_wrapper.h"
#include <qmath.h>
#include <qmetatype.h>
#include <QThread>
#include <QMetaObject>
#include <QDebug>

SpectrumAnalyserThread::SpectrumAnalyserThread(QObject *parent)
    :   QObject(parent)
    ,   m_fft(new FFTRealWrapper)
    ,   m_numSamples(SpectrumLengthSamples)
    ,   m_windowFunction(DefaultWindowFunction)
    ,   m_window(SpectrumLengthSamples, 0.0)
    ,   m_input(SpectrumLengthSamples, 0.0)
    ,   m_output(SpectrumLengthSamples, 0.0)
    ,   m_spectrum(SpectrumLengthSamples)
    ,   m_thread(new QThread())
{
    setParent(0);
    moveToThread(m_thread);
    m_thread->start();
    calculateWindow();
}

SpectrumAnalyserThread::~SpectrumAnalyserThread()
{
    delete m_fft;
    m_thread->quit();
    m_thread->deleteLater();
}

void SpectrumAnalyserThread::setWindowFunction(WindowFunction type)
{
    m_windowFunction = type;
    calculateWindow();
}

void SpectrumAnalyserThread::calculateWindow()
{
    int N=m_numSamples;
    for (int n=0; n<m_numSamples; ++n) {
        DataType x = 0.0;
        switch (m_windowFunction) {
        case NoWindow:
            x = 1.0;
            break;
        case GuassWindow:{
            float tmp=(n-(N-1)/2.0)/(0.4*(N-1)/2);
            x = exp(-0.5*(tmp*tmp));
            break;
        }
        case HannWindow:
            x = 0.5 * (1 - qCos((2 * M_PI * n) / (m_numSamples - 1)));
            break;
        case HammingWindow:
            x=0.53836-0.46164*qCos(2*M_PI*n/(N-1));
            break;
        case BartlettWindow:
            x=2.0/(N-1)*((N-1)/2.0-qAbs(n-(N-1)/2.0));
            break;
        case TriangleWindow:
            x=2.0/N*(N/2.0-qAbs(n-(N-1)/2.0));
            break;
        case BlackmanWindow:
            x=0.42-0.5*qCos(2*M_PI*n/(N-1))+0.08*cos(4*M_PI*n/(N-1));
            break;
        case NuttallWindow:
            x=0.355768 -0.487396*qCos(2*M_PI*n/(N-1))+0.1444232*qCos(4*M_PI*n/(N-1))+0.012604*qCos(6*M_PI*n/(N-1));
            break;
        case SinWindow:
            x=qSin(M_PI*n/(N-1));
            break;
        default:
            Q_ASSERT(false);
        }
        m_window[n] = x;
    }

}
void SpectrumAnalyserThread::calculateSpectrum(const QByteArray &buffer, int inputFrequency, int bitsPerSample, int channels)
{
    const int bytesPerSample =  bitsPerSample  / 8 * channels ;
    const char *ptr = buffer.constData();
    for (int i=0; i<m_numSamples; ++i) {
        const qint16 pcmSample = *reinterpret_cast<const qint16*>(ptr);
        const DataType realSample = pcmToReal(pcmSample);
        const DataType windowedSample = realSample * m_window[i];
        m_input[i] = windowedSample;
        ptr += bytesPerSample;
    }
    m_fft->calculateFFT(m_output.data(), m_input.data());
    for (int i = 0; i<m_numSamples/2; ++i) {
       m_spectrum[m_numSamples/2-1-i].frequency = - 2*qreal(i * inputFrequency) / (m_numSamples);
       const qreal real = m_output[i];
       qreal imag = 0.0;
       imag = m_output[m_numSamples/2 + i];
       const qreal magnitude = qSqrt(real*real + imag*imag);
       qreal amplitude = SpectrumAnalyserMultiplier * qLn(magnitude);
       m_spectrum[m_numSamples/2-1-i].clipped = (amplitude > 1.0);
       m_spectrum[m_numSamples/2-1-i].amplitude = qBound(0.0,amplitude,1.0);
    }
    if(channels==2){
        const int offset =  bytesPerSample/channels ;
        const char *ptr = buffer.constData();
        for (int i=0; i<m_numSamples; ++i) {
            const qint16 pcmSample = *reinterpret_cast<const qint16*>(ptr+offset);
            const DataType realSample = pcmToReal(pcmSample);
            const DataType windowedSample = realSample * m_window[i];
            m_input[i] = windowedSample;
            ptr += bytesPerSample;
        }
        m_fft->calculateFFT(m_output.data(), m_input.data());
        for (int i = 0; i<m_numSamples/2; ++i) {
           m_spectrum[m_numSamples/2+i].frequency = 2*qreal(i * inputFrequency) / (m_numSamples);
           const qreal real = m_output[i];
           qreal imag = 0.0;
           imag = m_output[m_numSamples/2 + i];
           const qreal magnitude = qSqrt(real*real + imag*imag);
           qreal amplitude = SpectrumAnalyserMultiplier * qLn(magnitude);
           m_spectrum[m_numSamples/2+i].clipped = (amplitude > 1.0);
           m_spectrum[m_numSamples/2+i].amplitude = qBound(0.0,amplitude,1.0);
        }
    }
    emit calculationComplete(m_spectrum);
}

SpectrumAnalyser::SpectrumAnalyser(QObject *parent)
    :   QObject(parent)
    ,   m_thread(new SpectrumAnalyserThread(this))
    ,   m_state(Idle)
{
    connect(m_thread, &SpectrumAnalyserThread::calculationComplete,
            this, &SpectrumAnalyser::calculationComplete);
}

SpectrumAnalyser::~SpectrumAnalyser()
{

}

void SpectrumAnalyser::setWindowFunction(WindowFunction type)
{
    QMetaObject::invokeMethod(m_thread, "setWindowFunction",
                              Qt::AutoConnection,
                              Q_ARG(WindowFunction, type));
}

void SpectrumAnalyser::calculate(const QByteArray &buffer,
                         const WAVEFORMATEX &format)
{
        m_state = Busy;
        QMetaObject::invokeMethod(m_thread, "calculateSpectrum",
                                  Qt::AutoConnection,
                                  Q_ARG(QByteArray, buffer),
                                  Q_ARG(int, format.nSamplesPerSec),
                                  Q_ARG(int, format.wBitsPerSample),
                                  Q_ARG(int,format.nChannels)               );
}

bool SpectrumAnalyser::isReady() const
{
    return (Idle == m_state);
}

void SpectrumAnalyser::cancelCalculation()
{
    if (Busy == m_state)
        m_state = Cancelled;
}

void SpectrumAnalyser::calculationComplete(const FrequencySpectrum &spectrum)
{
    Q_ASSERT(Idle != m_state);
    if (Busy == m_state)
        emit spectrumChanged(spectrum);
    m_state = Idle;
}
