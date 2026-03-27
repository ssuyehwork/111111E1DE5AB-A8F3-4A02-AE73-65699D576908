#ifndef THUMBNAIL_PROVIDER_H
#define THUMBNAIL_PROVIDER_H

#include <QString>
#include <QPixmap>
#include <QImage>
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

namespace ArcMeta {

class ThumbnailProvider {
public:
    static QPixmap getThumbnail(const QString& filePath, int size = 256) {
        Microsoft::WRL::ComPtr<IShellItem> shellItem;
        HRESULT hr = SHCreateItemFromParsingName(
            reinterpret_cast<const wchar_t*>(filePath.utf16()),
            nullptr, IID_PPV_ARGS(&shellItem)
        );

        if (FAILED(hr)) return QPixmap();

        Microsoft::WRL::ComPtr<IShellItemImageFactory> imageFactory;
        hr = shellItem.As(&imageFactory);
        if (FAILED(hr)) return QPixmap();

        SIZE sizeStruct = { size, size };
        HBITMAP hBitmap;
        hr = imageFactory->GetImage(sizeStruct, SIIGBF_RESIZETOFIT, &hBitmap);
        if (FAILED(hr)) return QPixmap();

        QImage image = fromHBitmap(hBitmap);
        DeleteObject(hBitmap);

        return QPixmap::fromImage(image);
    }

private:
    static QImage fromHBitmap(HBITMAP hBitmap) {
        BITMAP bitmap;
        GetObject(hBitmap, sizeof(BITMAP), &bitmap);

        QImage image(bitmap.bmWidth, bitmap.bmHeight, QImage::Format_ARGB32);

        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = bitmap.bmWidth;
        bmi.bmiHeader.biHeight = -bitmap.bmHeight; // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC hdc = GetDC(nullptr);
        GetDIBits(hdc, hBitmap, 0, bitmap.bmHeight, image.bits(), &bmi, DIB_RGB_COLORS);
        ReleaseDC(nullptr, hdc);

        return image;
    }
};

} // namespace ArcMeta

#endif // THUMBNAIL_PROVIDER_H
