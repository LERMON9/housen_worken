#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include <windows.ui.xaml.media.dxinterop.h>

using namespace D2D1;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;

namespace DisplayMetrics
{
	// Дисплеи высокого разрешения могут быть более требовательны к GPU и быстрее расходовать заряд батареи при отрисовке.
	// Телефоны с экраном высокого разрешения могут быстро расходовать заряд батареи, если
	// игры пытаются выполнить отрисовку на скорости в 60 кадров в секунду с полной точностью.
	// Решение об отрисовке с полной точностью для всех платформ и форм-факторов
	// должно быть обдуманным.
	static const bool SupportHighResolutions = false;

	// Пороговые значения по умолчанию, которые определяют дисплей "высокого разрешения". Если пороговые значения
	// превышены и SupportHighResolutions имеет значение False, размеры будут изменены
	// на 50 %.
	static const float DpiThreshold = 192.0f;		// 200 % стандартного рабочего стола.
	static const float WidthThreshold = 1920.0f;	// 1080p в ширину.
	static const float HeightThreshold = 1080.0f;	// 1080p в высоту.
};

// Константы, используемые для вычисления поворотов экрана.
namespace ScreenRotation
{
	// Поворот вокруг оси Z на 0 градусов
	static const XMFLOAT4X4 Rotation0( 
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Поворот вокруг оси Z на 90 градусов
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Поворот вокруг оси Z на 180 градусов
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Поворот вокруг оси Z на 270 градусов
	static const XMFLOAT4X4 Rotation270( 
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// Конструктор для DeviceResources.
DX::DeviceResources::DeviceResources() : 
	m_screenViewport(),
	m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_compositionScaleX(1.0f),
	m_compositionScaleY(1.0f),
	m_deviceNotify(nullptr)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Настраивает ресурсы, которые не зависят от устройства Direct3D.
void DX::DeviceResources::CreateDeviceIndependentResources()
{
	// Инициализация ресурсов Direct2D.
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
	// Если проект не является отладочной сборкой, включите отладку Direct2D с помощью уровней SDK.
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Инициализация фабрики Direct2D.
	DX::ThrowIfFailed(
		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory3),
			&options,
			&m_d2dFactory
			)
		);

	// Инициализация фабрики DirectWrite.
	DX::ThrowIfFailed(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory3),
			&m_dwriteFactory
			)
		);

	// Инициализация фабрики компонента обработки изображений Windows (WIC).
	DX::ThrowIfFailed(
		CoCreateInstance(
			CLSID_WICImagingFactory2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_wicFactory)
			)
		);
}

// Настраивает устройство Direct3D и сохраняет его дескрипторы и контекст устройства.
void DX::DeviceResources::CreateDeviceResources() 
{
	// Этот флаг добавляет поддержку поверхностей с порядком цветовых каналов, отличным
	// от предусмотренного в API по умолчанию. Он является обязательным для совместимости с Direct2D.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (DX::SdkLayersAvailable())
	{
		// Если проект находится в отладочной сборке, включите отладку посредством уровней SDK с помощью этого флага.
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	// Этот массив определяет набор функциональных уровней оборудования DirectX, которые будет поддерживать данное приложение.
	// Обратите внимание, что необходимо сохранить порядок.
	// Не забудьте объявить минимальный требуемый функциональный уровень вашего приложения в его
	// описании.  Предполагается, что все приложения поддерживают уровень 9.1, если не указано иное.
	D3D_FEATURE_LEVEL featureLevels[] = 
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	// Создание объекта устройства API Direct3D 11 и соответствующего контекста.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	HRESULT hr = D3D11CreateDevice(
		nullptr,					// Указание nullptr для использования адаптера по умолчанию.
		D3D_DRIVER_TYPE_HARDWARE,	// Создание устройства с помощью драйвера графического оборудования.
		0,							// Должно равняться 0, если драйвер не равен D3D_DRIVER_TYPE_SOFTWARE.
		creationFlags,				// Установка флагов отладки и совместимости с Direct2D.
		featureLevels,				// Список функциональных уровней, которые могут поддерживаться этим приложением.
		ARRAYSIZE(featureLevels),	// Размер вышеприведенного списка.
		D3D11_SDK_VERSION,			// Всегда задавайте D3D11_SDK_VERSION для приложений Microsoft Store.
		&device,					// Возвращает созданное устройство Direct3D.
		&m_d3dFeatureLevel,			// Возвращает уровень возможностей созданного устройства.
		&context					// Возвращает мгновенный контекст устройства.
		);

	if (FAILED(hr))
	{
		// В случае сбоя инициализации выполните откат до устройства WARP.
		// Дополнительные сведения о WARP см. на сайте 
		// https://go.microsoft.com/fwlink/?LinkId=286690
		DX::ThrowIfFailed(
			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP, // Создание устройства WARP вместо аппаратного устройства.
				0,
				creationFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				&device,
				&m_d3dFeatureLevel,
				&context
				)
			);
	}

	// Сохранение указателей на устройство API Direct3D 11.3 и в окружающий контекст.
	DX::ThrowIfFailed(
		device.As(&m_d3dDevice)
		);

	DX::ThrowIfFailed(
		context.As(&m_d3dContext)
		);

	// Создание объекта устройства Direct2D и соответствующего контекста.
	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(
		m_d3dDevice.As(&dxgiDevice)
		);

	DX::ThrowIfFailed(
		m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice)
		);

	DX::ThrowIfFailed(
		m_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			&m_d2dContext
			)
		);
}

// Эти ресурсы необходимо заново создавать при каждом изменении размера окна.
void DX::DeviceResources::CreateWindowSizeDependentResources() 
{
	// Очистка контекста, связанного с размером предыдущего окна.
	ID3D11RenderTargetView* nullViews[] = {nullptr};
	m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
	m_d3dRenderTargetView = nullptr;
	m_d2dContext->SetTarget(nullptr);
	m_d2dTargetBitmap = nullptr;
	m_d3dDepthStencilView = nullptr;
	m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

	UpdateRenderTargetSize();

	// Ширина и высота цепочки буферов должны зависеть от
	// ширины и высоты окна в собственной ориентации. Если окно не находится в собственной
	// ориентации, размеры необходимо поменять местами.
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	if (m_swapChain != nullptr)
	{
		// Если цепочка буферов уже существует, измените ее размер.
		HRESULT hr = m_swapChain->ResizeBuffers(
			2, // Цепочка буферов с двойной буферизацией.
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// Если устройство было по какой-либо причине удалено, необходимо создать новое устройство и цепочку буферов.
			HandleDeviceLost();

			// Все параметры настроены. Не продолжайте выполнение этого метода. HandleDeviceLost перезапустит этот метод 
			// и правильно настроит новое устройство.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// В противном случае создайте новое устройство, используя тот же адаптер, что и имеющее устройство Direct3D.
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// Сопоставление с размером окна.
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// Это наиболее распространенный формат цепочки буферов.
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// Не используйте множественную дискретизацию.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;									// Использование двойной буферизации, чтобы свести задержку к минимуму.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// Все приложения Microsoft Store должны использовать _FLIP_ SwapEffects.
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// Эта последовательность получает фабрику DXGI, которая использовалась для создания вышеуказанного устройства Direct3D.
		ComPtr<IDXGIDevice3> dxgiDevice;
		DX::ThrowIfFailed(
			m_d3dDevice.As(&dxgiDevice)
			);

		ComPtr<IDXGIAdapter> dxgiAdapter;
		DX::ThrowIfFailed(
			dxgiDevice->GetAdapter(&dxgiAdapter)
			);

		ComPtr<IDXGIFactory4> dxgiFactory;
		DX::ThrowIfFailed(
			dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
			);

		// При использовании взаимодействия с XAML необходимо создать для композиции цепочку буферов.
		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			dxgiFactory->CreateSwapChainForComposition(
				m_d3dDevice.Get(),
				&swapChainDesc,
				nullptr,
				&swapChain
				)
			);

		DX::ThrowIfFailed(
			swapChain.As(&m_swapChain)
			);

		// Свяжите цепочку буферов с объектом SwapChainPanel
		// Необходимо отправить изменения пользовательского интерфейса обратно в поток пользовательского интерфейса
		m_swapChainPanel->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([=]()
		{
			// Получение резервного собственного интерфейса для SwapChainPanel
			ComPtr<ISwapChainPanelNative> panelNative;
			DX::ThrowIfFailed(
				reinterpret_cast<IUnknown*>(m_swapChainPanel)->QueryInterface(IID_PPV_ARGS(&panelNative))
				);

			DX::ThrowIfFailed(
				panelNative->SetSwapChain(m_swapChain.Get())
				);
		}, CallbackContext::Any));

		// Проверка того, что DXGI не помещает в очередь более одного кадра одновременно. Это позволяет уменьшить задержку и
		// гарантировать, что приложение будет выполнять прорисовку только после каждой вертикальной синхронизации, что снижает энергопотребление.
		DX::ThrowIfFailed(
			dxgiDevice->SetMaximumFrameLatency(1)
			);
	}

	// Установка правильной ориентации цепочки буферов и создание двумерных и
	// трехмерных матричных преобразований для прорисовки повернутой цепочки буферов.
	// Обратите внимание, что углы поворота для двумерных и трехмерных преобразований различаются.
	// Это связано с различием в пространствах координат.  Кроме того,
	// трехмерная матрица задается явным образом, чтобы избежать ошибок округления.

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform2D = Matrix3x2F::Identity();
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(90.0f) *
			Matrix3x2F::Translation(m_logicalSize.Height, 0.0f);
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(180.0f) *
			Matrix3x2F::Translation(m_logicalSize.Width, m_logicalSize.Height);
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(270.0f) *
			Matrix3x2F::Translation(0.0f, m_logicalSize.Width);
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
		);

	// Настройка инверсной шкалы в цепочке буферов
	DXGI_MATRIX_3X2_F inverseScale = { 0 };
	inverseScale._11 = 1.0f / m_effectiveCompositionScaleX;
	inverseScale._22 = 1.0f / m_effectiveCompositionScaleY;
	ComPtr<IDXGISwapChain2> spSwapChain2;
	DX::ThrowIfFailed(
		m_swapChain.As<IDXGISwapChain2>(&spSwapChain2)
		);

	DX::ThrowIfFailed(
		spSwapChain2->SetMatrixTransform(&inverseScale)
		);

	// Создание представления целевого объекта прорисовки для заднего буфера цепочки буферов.
	ComPtr<ID3D11Texture2D1> backBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
		);

	DX::ThrowIfFailed(
		m_d3dDevice->CreateRenderTargetView1(
			backBuffer.Get(),
			nullptr,
			&m_d3dRenderTargetView
			)
		);

	// Создание представления трафарета глубины для использования с трехмерной прорисовкой, если это необходимо.
	CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
		DXGI_FORMAT_D24_UNORM_S8_UINT, 
		lround(m_d3dRenderTargetSize.Width),
		lround(m_d3dRenderTargetSize.Height),
		1, // Это представление трафарета глубины содержит только одну текстуру.
		1, // Использование одного уровня MIP-карт.
		D3D11_BIND_DEPTH_STENCIL
		);

	ComPtr<ID3D11Texture2D1> depthStencil;
	DX::ThrowIfFailed(
		m_d3dDevice->CreateTexture2D1(
			&depthStencilDesc,
			nullptr,
			&depthStencil
			)
		);

	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
	DX::ThrowIfFailed(
		m_d3dDevice->CreateDepthStencilView(
			depthStencil.Get(),
			&depthStencilViewDesc,
			&m_d3dDepthStencilView
			)
		);

	// Установка в качестве окна просмотра трехмерной прорисовки всего окна.
	m_screenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		m_d3dRenderTargetSize.Width,
		m_d3dRenderTargetSize.Height
		);

	m_d3dContext->RSSetViewports(1, &m_screenViewport);

	// Создание целевого точечного рисунка Direct2D, связанного с
	// задним буфером цепочки буферов, и установка его в качестве текущего целевого объекта.
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = 
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			m_dpi,
			m_dpi
			);

	ComPtr<IDXGISurface2> dxgiBackBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
		);

	DX::ThrowIfFailed(
		m_d2dContext->CreateBitmapFromDxgiSurface(
			dxgiBackBuffer.Get(),
			&bitmapProperties,
			&m_d2dTargetBitmap
			)
		);

	m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
	m_d2dContext->SetDpi(m_effectiveDpi, m_effectiveDpi);

	// Сглаживание текста в оттенках серого рекомендуется для всех приложений Microsoft Store.
	m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

// Определите размеры целевого объекта отрисовки и необходимость уменьшения его масштаба.
void DX::DeviceResources::UpdateRenderTargetSize()
{
	m_effectiveDpi = m_dpi;
	m_effectiveCompositionScaleX = m_compositionScaleX;
	m_effectiveCompositionScaleY = m_compositionScaleY;

	// Чтобы продлить время использования заряда батареи на устройствах с экраном высокого разрешения, отрисовку следует выполнять для меньших целевых объектов
	// и разрешите GPU изменять масштаб вывода, когда он отображается.
	if (!DisplayMetrics::SupportHighResolutions && m_dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
		float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

		// Когда устройство работает в книжной ориентации, высота больше ширины. Сравните
		// больший размер с пороговым значением ширины и меньший размер
		// с пороговым значением высоты.
		if (max(width, height) > DisplayMetrics::WidthThreshold && min(width, height) > DisplayMetrics::HeightThreshold)
		{
			// Чтобы изменить масштаб приложения, изменяется фактическое значение DPI. Логический размер не меняется.
			m_effectiveDpi /= 2.0f;
			m_effectiveCompositionScaleX /= 2.0f;
			m_effectiveCompositionScaleY /= 2.0f;
		}
	}

	// Вычислите необходимый размер в пикселях целевого объекта отрисовки.
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// Предотвращение создания содержимого DirectX с нулевым размером.
	m_outputSize.Width = max(m_outputSize.Width, 1);
	m_outputSize.Height = max(m_outputSize.Height, 1);
}

// Этот метод вызывается при создании или повторном создании элемента управления XAML.
void DX::DeviceResources::SetSwapChainPanel(SwapChainPanel^ panel)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_swapChainPanel = panel;
	m_logicalSize = Windows::Foundation::Size(static_cast<float>(panel->ActualWidth), static_cast<float>(panel->ActualHeight));
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	m_compositionScaleX = panel->CompositionScaleX;
	m_compositionScaleY = panel->CompositionScaleY;
	m_dpi = currentDisplayInformation->LogicalDpi;
	m_d2dContext->SetDpi(m_dpi, m_dpi);

	CreateWindowSizeDependentResources();
}

// Этот метод вызывается в обработчике события SizeChanged.
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// Этот метод вызывается в обработчике события DpiChanged.
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;
		m_d2dContext->SetDpi(m_dpi, m_dpi);
		CreateWindowSizeDependentResources();
	}
}

// Этот метод вызывается в обработчике события OrientationChanged.
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// Этот метод вызывается в обработчике событий для события CompositionScaleChanged.
void DX::DeviceResources::SetCompositionScale(float compositionScaleX, float compositionScaleY)
{
	if (m_compositionScaleX != compositionScaleX ||
		m_compositionScaleY != compositionScaleY)
	{
		m_compositionScaleX = compositionScaleX;
		m_compositionScaleY = compositionScaleY;
		CreateWindowSizeDependentResources();
	}
}

// Этот метод вызывается в обработчике события DisplayContentsInvalidated.
void DX::DeviceResources::ValidateDevice()
{
	// Устройство D3D больше не является допустимым, если адаптер по умолчанию был изменен с момента
	// создания устройства или если устройство было удалено.

	// Сначала необходимо получить информацию об адаптере по умолчанию на момент создания устройства.

	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

	ComPtr<IDXGIAdapter> deviceAdapter;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

	ComPtr<IDXGIFactory2> deviceFactory;
	DX::ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	ComPtr<IDXGIAdapter1> previousDefaultAdapter;
	DX::ThrowIfFailed(deviceFactory->EnumAdapters1(0, &previousDefaultAdapter));

	DXGI_ADAPTER_DESC1 previousDesc;
	DX::ThrowIfFailed(previousDefaultAdapter->GetDesc1(&previousDesc));

	// Затем следует получить информацию о текущем адаптере по умолчанию.

	ComPtr<IDXGIFactory4> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC1 currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc1(&currentDesc));

	// Если LUID адаптеров не совпадают или устройство сообщает, что оно было удалено,
	// необходимо создать новое устройство D3D.

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		// Освобождение ссылок на ресурсы, связанные со старым устройством.
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		// Создание нового устройства и цепочки буферов.
		HandleDeviceLost();
	}
}

// Повторное создание всех ресурсов устройства и их возврат к текущему состоянию.
void DX::DeviceResources::HandleDeviceLost()
{
	m_swapChain = nullptr;

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

	CreateDeviceResources();
	m_d2dContext->SetDpi(m_dpi, m_dpi);
	CreateWindowSizeDependentResources();

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
}

// Регистрация DeviceNotify для получения уведомлений о потере и создании устройства.
void DX::DeviceResources::RegisterDeviceNotify(DX::IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

// Этот метод вызывается при приостановке приложения. Он содержит указание драйверу, что приложение 
// переходит в состояние бездействия и что временные буферы можно освободить для использования другими приложениями.
void DX::DeviceResources::Trim()
{
	ComPtr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);

	dxgiDevice->Trim();
}

// Вывод содержимого цепочки буферов на экран.
void DX::DeviceResources::Present() 
{
	// Первый аргумент указывает DXGI выполнять блокировку до вертикальной синхронизации, переводя приложение
	// в спящий режим до следующей вертикальной синхронизации. Это позволит избежать циклов прорисовки
	// кадров, которые никогда не будут отображаться на экране.
	DXGI_PRESENT_PARAMETERS parameters = { 0 };
	HRESULT hr = m_swapChain->Present1(1, 0, &parameters);

	// Удаление содержимого целевого объекта прорисовки.
	// Эта операция допустима, только если имеющееся содержимое будет полностью
	// перезаписано. Если используются прямоугольники "dirty" или "scroll", этот вызов необходимо изменить.
	m_d3dContext->DiscardView1(m_d3dRenderTargetView.Get(), nullptr, 0);

	// Удаление содержимого трафарета глубины.
	m_d3dContext->DiscardView1(m_d3dDepthStencilView.Get(), nullptr, 0);

	// Если устройство было удалено в результате отключения или обновления драйвера, 
	// необходимо заново создать все ресурсы устройства.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HandleDeviceLost();
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}
}

// Этот метод определяет поворот между собственной ориентацией устройства и
// текущей ориентацией экрана.
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// Примечание. Параметр NativeOrientation может иметь только значения Landscape и Portrait, даже если
	// перечисление DisplayOrientations содержит другие значения.
	switch (m_nativeOrientation)
	{
	case DisplayOrientations::Landscape:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;
		}
		break;

	case DisplayOrientations::Portrait:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;
		}
		break;
	}
	return rotation;
}