#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <QMap>
#include <windows.h>
#include <QObject>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <QThread>

class IMMDeviceEnumerator;
class IMMDevice;
class IAudioCaptureClient;
class IAudioClient;

class CaptureAudioThread: public QObject
{
    Q_OBJECT
public:
    CaptureAudioThread();
    ~CaptureAudioThread();
    Q_INVOKABLE void start(IAudioClient*);
    void stop();
    bool isRunning() const;
private:
    QThread *thread;
    bool bRunning;
private:
    inline void inspect(HRESULT hr);
signals:
    void notifyData(const QByteArray &);
    void stoped();
    void error(HRESULT);
};

struct CaptureDevice{
    QString name;
    _EDataFlow type;
    LPWSTR id;
};

class AudioCapture : public QObject,public IMMNotificationClient
{
    Q_OBJECT
public:
    explicit AudioCapture(QObject *parent = nullptr);
    ~AudioCapture();
    Q_INVOKABLE void startCapture();
    Q_INVOKABLE void stopCapture();
    bool isRunning() const;
    void flushDevices();
    QList<CaptureDevice> getDevices() const;
    QString getCurrentDevice() const;

    QString getDeviceName(LPCWSTR id);
    void setCurrentDevice(const QString &value);

protected:
    //以下函数用于监视设备变化
    ULONG STDMETHODCALLTYPE AddRef(){return 0;};

    ULONG STDMETHODCALLTYPE Release(){return 0;};

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID, VOID**){return 0;}

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(
        EDataFlow flow, ERole role,
            LPCWSTR pwstrDeviceId);

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId);

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId);

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD){return 0;}

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged( LPCWSTR, const PROPERTYKEY){return 0;}
    BOOL AdjustFormatTo16Bits(WAVEFORMATEX *pwfx);

private:
    IMMDevice           *pDevice;
    IMMDeviceEnumerator *pInputEnumerator;
    IMMDeviceEnumerator *pOutputEnumerator;
    IAudioClient        *pAudioClient;
    WAVEFORMATEX        *pwfx;
    QList<CaptureDevice> devices;
    QString currentDevice;
    CaptureAudioThread thread;
private:
    CaptureDevice* getDevice(QString name);
    inline void inspect(HRESULT hr);

signals:
    void notifyData(const QByteArray &);
    void formatChanged(WAVEFORMATEX fromat);
};

#endif // QCAPTURE_H
