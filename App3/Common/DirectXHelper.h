#pragma once

#include <ppltasks.h>	// Для create_task

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Установка точки останова на этой строке для перехвата ошибок API Win32.
			throw Platform::Exception::CreateException(hr);
		}
	}

	// Функция, осуществляющая асинхронное чтение из двоичного файла.
	inline Concurrency::task<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
	{
		using namespace Windows::Storage;
		using namespace Concurrency;

		auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

		return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([] (StorageFile^ file) 
		{
			return FileIO::ReadBufferAsync(file);
		}).then([] (Streams::IBuffer^ fileBuffer) -> std::vector<byte> 
		{
			std::vector<byte> returnBuffer;
			returnBuffer.resize(fileBuffer->Length);
			Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer.data(), fileBuffer->Length));
			return returnBuffer;
		});
	}

	// Преобразует длину в аппаратно-независимых пикселях (DIP) в длину в физических пикселях.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // Округление до ближайшего целого.
	}

#if defined(_DEBUG)
	// Проверка поддержки уровней пакета SDK.
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // Нет необходимости создавать настоящее аппаратное устройство.
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // Проверка уровней пакета SDK.
			nullptr,                    // Подойдет любой функциональный уровень.
			0,
			D3D11_SDK_VERSION,          // Всегда задавайте D3D11_SDK_VERSION для приложений Microsoft Store.
			nullptr,                    // Нет необходимости сохранять ссылку на устройство D3D.
			nullptr,                    // Нет необходимости знать функциональный уровень.
			nullptr                     // Нет необходимости сохранять ссылку на контекст устройства D3D.
			);

		return SUCCEEDED(hr);
	}
#endif
}
