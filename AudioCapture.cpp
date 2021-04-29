#include "AudioCapture.h"
#include <audioclient.h>
#include <process.h>
#include <setupapi.h>
#include <initguid.h>
#include <functiondiscoverykeys_devpkey.h>
#include <QEventLoop>
#include <QDebug>

CaptureAudioThread::CaptureAudioThread()
    : thread(new QThread())
    , bRunning(false)
{
    moveToThread(thread);
    thread->start();
}

CaptureAudioThread::~CaptureAudioThread()
{
    thread->quit();
    thread->deleteLater();
}

void CaptureAudioThread::start(IAudioClient* pAudioClient)
{
    CoInitialize(NULL);
    IAudioCaptureClient *pCaptureClient;
    UINT32 numFramesAvailable;
    UINT32 packetLength = 0;
    BYTE *pData;
    DWORD flags;
    inspect(pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient));
    inspect(pAudioClient->Start());
    bRunning=true;
    QByteArray data;
    qDebug()<<"频谱引擎已启动";
    while (bRunning) {
        QThread::msleep(5);
        inspect(pCaptureClient->GetNextPacketSize(&packetLength));
        if(packetLength!=0){
            inspect(pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL));
            int dataSize = numFramesAvailable*4;
            data.resize(dataSize);
            memcpy_s(data.data(), dataSize, pData, dataSize);
            emit notifyData(data);
            inspect(pCaptureClient->ReleaseBuffer(numFramesAvailable));
        }
    }
    inspect(pAudioClient->Stop());
    emit stoped();
    qDebug()<<"频谱引擎已停止采集！";
}

void CaptureAudioThread::stop()
{
    bRunning=false;
}

bool CaptureAudioThread::isRunning() const
{
    return bRunning;
}

void CaptureAudioThread::inspect(HRESULT hr)
{
    if(FAILED(hr))
        emit error(hr);
}

AudioCapture::AudioCapture(QObject *parent)
    : QObject(parent)
    , pDevice(nullptr)
    , pInputEnumerator(nullptr)
    , pOutputEnumerator(nullptr)
    , pAudioClient(nullptr)
    , pwfx(nullptr)
    , currentDevice("默认输出设备")
{
    qRegisterMetaType<IAudioClient*>("IAudioClient*");
    qRegisterMetaType<HRESULT>("HRESULT");

    inspect(CoInitialize(NULL));
    connect(&thread,&CaptureAudioThread::notifyData,this,[this](const QByteArray& data){emit notifyData(data);});
    connect(&thread,&CaptureAudioThread::error,this,&AudioCapture::inspect);
    inspect(CoCreateInstance(CLSID_MMDeviceEnumerator,
                              NULL,
                              CLSCTX_ALL,
                              IID_IMMDeviceEnumerator,
                              (void**)&pOutputEnumerator));
    pOutputEnumerator->RegisterEndpointNotificationCallback(this);
    flushDevices();
}

AudioCapture::~AudioCapture()
{


}

void AudioCapture::flushDevices()
{
    devices.clear();
    IMMDeviceCollection *pCollection=NULL;
    inspect(pOutputEnumerator->EnumAudioEndpoints(eRender,eMultimedia,&pCollection));
    UINT count;
    LPWSTR pwszID = NULL;
    inspect(pCollection->GetCount(&count));
    devices.push_back({"默认输出设备",eRender,0});

    for(UINT i=0;i<count;i++){
        inspect(pCollection->Item(i, &pDevice));
        inspect(pDevice->GetId(&pwszID));
        devices.push_back({getDeviceName(pwszID),eRender,pwszID});
    }

//    devices.push_back({"默认输入设备",eCapture,0});
//    inspect(CoCreateInstance(CLSID_MMDeviceEnumerator,NULL,
//                              CLSCTX_ALL,
//                              IID_IMMDeviceEnumerator,
//                              (void**)&pInputEnumerator));
//    inspect(pInputEnumerator->EnumAudioEndpoints(eCapture,eMultimedia,&pCollection));
//    inspect(pCollection->GetCount(&count));
//    for(UINT i=0;i<count;i++){
//        inspect(pCollection->Item(i, &pDevice));
//        inspect(pDevice->GetId(&pwszID));
////      devices.push_back({getDeviceName(pwszID),eCapture,pwszID});
//    }


}

void AudioCapture::startCapture()
{
    CoInitialize(NULL);
    if(thread.isRunning()){
        QEventLoop loop;
        connect(&thread,&CaptureAudioThread::stoped,&loop,&QEventLoop::quit);
        thread.stop();
        loop.exec();
    }
    REFERENCE_TIME hnsRequestedDuration = 10000000;
    UINT32 bufferFrameCount;
    auto device=getDevice(currentDevice);
    if(device==nullptr){
        pOutputEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    }
    if(device->name=="默认输出设备"){
        pOutputEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    }
//    else if(device->name=="默认输入设备"){
//        pInputEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pDevice);
//    }
    else if(device->type==eRender){
        pOutputEnumerator->GetDevice(device->id,&pDevice);

    }
    else if(device->type==eCapture){
        pInputEnumerator->GetDevice(device->id,&pDevice);
    }
    

    inspect(pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient));
    inspect(pAudioClient->GetMixFormat(&pwfx));
    AdjustFormatTo16Bits(pwfx);
    emit formatChanged(*pwfx);


    qDebug()<<"音频设备——（"+currentDevice+"）已就绪！";
    qDebug()<<"通道数 : "<<pwfx->nChannels;
    qDebug()<<"采样率 : "<<pwfx->nSamplesPerSec;
    qDebug()<<"位深  : "<<pwfx->wBitsPerSample;
    inspect(pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            device->type==eRender?
                    AUDCLNT_STREAMFLAGS_LOOPBACK
                  : (AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST),
            hnsRequestedDuration,
            0,
            pwfx,
            NULL));

    inspect(pAudioClient->GetBufferSize(&bufferFrameCount));

    QMetaObject::invokeMethod(&thread,"start",Qt::AutoConnection,Q_ARG(IAudioClient*,pAudioClient));

}


BOOL AudioCapture::AdjustFormatTo16Bits(WAVEFORMATEX *pwfx)
{
    bool bRet(FALSE);
    pwfx->wFormatTag = WAVE_FORMAT_PCM;
    pwfx->wBitsPerSample = 16;
    pwfx->nChannels = 2;
    pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
    pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
    pwfx->cbSize=0;
    bRet=TRUE;
    return bRet;
}

void AudioCapture::stopCapture()
{
    thread.stop();
}

bool AudioCapture::isRunning() const
{
    return thread.isRunning();
}

CaptureDevice *AudioCapture::getDevice(QString name)
{
    for(auto&it:devices){
        if(it.name==name)
            return &it;
    }
    return nullptr;
}

HRESULT AudioCapture::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
{
    QString name=getDeviceName(pwstrDeviceId);
    if(role==eMultimedia){
        if(flow==eRender&&currentDevice=="默认输出设备"){
            flushDevices();
            for(auto &device:devices){
                if(device.name==name){
                    setCurrentDevice(name);
                    currentDevice="默认输出设备";
                    return 0;
                }
            }
        }
//        else if(flow==eCapture&&currentDevice=="默认输入设备"){
//            for(auto &device:devices){
//                if(device.name==name){
//                    setCurrentDevice(name);
//                    currentDevice="默认输入设备";
//                    return 0;
//                }
//            }
//        }
    }
    return 0;
}

HRESULT AudioCapture::OnDeviceAdded(LPCWSTR pwstrDeviceId)
{
    qDebug()<<"设备已添加"<<pwstrDeviceId;
    return 0l;
}

HRESULT AudioCapture::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
    qDebug()<<"设备已移除"<<pwstrDeviceId;
    return 0l;
}

QString AudioCapture::getCurrentDevice() const
{
    return currentDevice;
}

QString AudioCapture::getDeviceName(LPCWSTR id)
{

    IPropertyStore* pProps = NULL;
    PROPVARIANT varName;
    IMMDevice *tmp;
    pOutputEnumerator->GetDevice(id,&tmp);
    if(tmp!=nullptr){
        pOutputEnumerator->GetDevice(id,&tmp);
    }
    else
        return "默认输出设备";
    inspect(tmp->OpenPropertyStore(
                STGM_READ, &pProps));
    PropVariantInit(&varName);
    inspect(pProps->GetValue(
            PKEY_Device_FriendlyName, &varName));
    QString name=QString("%1").arg(varName.pwszVal);
    PropVariantClear(&varName);
    return name;
}

void AudioCapture::setCurrentDevice(const QString &value)
{
    currentDevice = value;
    if(isRunning()){
        startCapture();
    }
}

QList<CaptureDevice> AudioCapture::getDevices() const
{
    return devices;
}

void AudioCapture::inspect(HRESULT hr)
{
    if(FAILED(hr)){
        qDebug()<<"引擎启动失败，错误代码"<<QString::number(hr,16);
        if(hr==AUDCLNT_E_DEVICE_INVALIDATED){
            setCurrentDevice("默认输出设备");
        }
    }
}


