#include "wicdecode.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincodec.h>

#include <QDebug>
#include <QImageWriter>

namespace kandu {

namespace {
HRESULT initCom()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE)
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    return hr; // S_FALSE 表示已初始化，也视为成功
}
} // namespace

QImage wicLoadImage(const QString &path)
{
    const HRESULT init = initCom();
    if (FAILED(init))
        return {};

    IWICImagingFactory *factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        if (init == S_OK)
            CoUninitialize();
        return {};
    }

    IWICBitmapDecoder *decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(
        reinterpret_cast<LPCWSTR>(path.utf16()), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnDemand, &decoder);
    QImage result;
    if (SUCCEEDED(hr)) {
        IWICBitmapFrameDecode *frame = nullptr;
        hr = decoder->GetFrame(0, &frame);
        if (SUCCEEDED(hr)) {
            UINT w = 0, h = 0;
            frame->GetSize(&w, &h);
            if (w > 0 && h > 0 && w <= 32768 && h <= 32768) {
                IWICFormatConverter *conv = nullptr;
                hr = factory->CreateFormatConverter(&conv);
                if (SUCCEEDED(hr)) {
                    hr = conv->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                                          WICBitmapDitherTypeNone, nullptr, 0,
                                          WICBitmapPaletteTypeCustom);
                    if (SUCCEEDED(hr)) {
                        const UINT stride = w * 4;
                        QByteArray buf(int(stride) * int(h), Qt::Uninitialized);
                        hr = conv->CopyPixels(nullptr, stride, UINT(buf.size()),
                                              reinterpret_cast<BYTE *>(buf.data()));
                        if (SUCCEEDED(hr)) {
                            // QImage 引用外部缓冲，深拷贝为独立内存
                            QImage tmp(reinterpret_cast<const uchar *>(buf.constData()),
                                       int(w), int(h), int(stride),
                                       QImage::Format_ARGB32);
                            result = tmp.copy();
                        }
                    }
                    conv->Release();
                }
            }
            frame->Release();
        }
        decoder->Release();
    }
    factory->Release();
    if (init == S_OK)
        CoUninitialize();
    return result;
}

bool qtCanEncode(const QString &extension)
{
    const QString ext = extension.toLower();
    if (ext.isEmpty())
        return false;
    const QString fmt = ext.startsWith(QLatin1Char('.')) ? ext.mid(1) : ext;
    return QImageWriter::supportedImageFormats().contains(fmt.toUtf8());
}

} // namespace kandu
