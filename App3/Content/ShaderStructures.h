#pragma once

namespace App3
{
	// Буфер констант, используемый для отправки матриц MVP в шейдер вершин.
	struct ModelViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	// Используется для отправки данных отдельных вершин в шейдер вершин.
	struct VertexPositionColor
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 color;
	};
}