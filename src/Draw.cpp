#include "Draw.h"

#include "Collision.h"
#include "Fade.h"
#include "Settings.h"

#include <MinHook.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace mcc::draw
{
	namespace
	{
		using Microsoft::WRL::ComPtr;

		using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
		PresentFn g_originalPresent = nullptr;

		struct Vertex
		{
			float x, y;        // clip space
			float r, g, b, a;
		};

		// One pipeline for lines in clip space, made on first use.
		ComPtr<ID3D11VertexShader> g_vertexShader;
		ComPtr<ID3D11PixelShader>  g_pixelShader;
		ComPtr<ID3D11InputLayout>  g_layout;
		ComPtr<ID3D11Buffer>       g_vertices;
		ComPtr<ID3D11BlendState>   g_blend;
		ComPtr<ID3D11RasterizerState> g_raster;
		ComPtr<ID3D11DepthStencilState> g_noDepth;
		constexpr UINT             kMaxVertices = 65536;
		bool                       g_pipelineFailed = false;

		constexpr const char* kShader = R"(
struct VSIn  { float2 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };
VSOut vs(VSIn i) { VSOut o; o.pos = float4(i.pos, 0.0, 1.0); o.col = i.col; return o; }
float4 ps(VSOut i) : SV_TARGET { return i.col; }
)";

		bool MakePipeline(ID3D11Device* a_device)
		{
			if (g_vertexShader) {
				return true;
			}
			if (g_pipelineFailed) {
				return false;
			}
			ComPtr<ID3DBlob> vs, ps, errors;
			if (FAILED(D3DCompile(kShader, std::strlen(kShader), "mcc_lines", nullptr, nullptr, "vs", "vs_4_0", 0, 0, &vs, &errors)) ||
				FAILED(D3DCompile(kShader, std::strlen(kShader), "mcc_lines", nullptr, nullptr, "ps", "ps_4_0", 0, 0, &ps, &errors))) {
				spdlog::error("draw: the line shader did not compile: {}",
					errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no message");
				g_pipelineFailed = true;
				return false;
			}
			const D3D11_INPUT_ELEMENT_DESC elements[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			D3D11_BUFFER_DESC buffer{};
			buffer.ByteWidth = kMaxVertices * sizeof(Vertex);
			buffer.Usage = D3D11_USAGE_DYNAMIC;
			buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			D3D11_BLEND_DESC blend{};
			blend.RenderTarget[0].BlendEnable = TRUE;
			blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			D3D11_RASTERIZER_DESC raster{};
			raster.FillMode = D3D11_FILL_SOLID;
			raster.CullMode = D3D11_CULL_NONE;
			raster.DepthClipEnable = FALSE;
			raster.ScissorEnable = FALSE;
			D3D11_DEPTH_STENCIL_DESC depth{};
			depth.DepthEnable = FALSE;
			depth.StencilEnable = FALSE;

			if (FAILED(a_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_vertexShader)) ||
				FAILED(a_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_pixelShader)) ||
				FAILED(a_device->CreateInputLayout(elements, 2, vs->GetBufferPointer(), vs->GetBufferSize(), &g_layout)) ||
				FAILED(a_device->CreateBuffer(&buffer, nullptr, &g_vertices)) ||
				FAILED(a_device->CreateBlendState(&blend, &g_blend)) ||
				FAILED(a_device->CreateRasterizerState(&raster, &g_raster)) ||
				FAILED(a_device->CreateDepthStencilState(&depth, &g_noDepth))) {
				spdlog::error("draw: the line pipeline could not be made");
				g_pipelineFailed = true;
				return false;
			}
			spdlog::info("draw: line pipeline ready");
			return true;
		}

		// --- projection -------------------------------------------------------------
		float g_width = 1.0f, g_height = 1.0f;

		bool ToScreen(const RE::NiPoint3& a_point, float& a_x, float& a_y)
		{
			auto* playerCamera = RE::PlayerCamera::GetSingleton();
			if (!playerCamera || !playerCamera->cameraRoot || playerCamera->cameraRoot->GetChildren().empty()) {
				return false;
			}
			auto* camera = netimmerse_cast<RE::NiCamera*>(playerCamera->cameraRoot->GetChildren()[0].get());
			if (!camera) {
				return false;
			}
			float x = 0.0f, y = 0.0f, z = 0.0f;
			if (!camera->WorldPtToScreenPt3(a_point, x, y, z, 1e-5f) || z <= 0.0f) {
				return false;
			}
			a_x = x * 2.0f - 1.0f;
			a_y = y * 2.0f - 1.0f;  // the engine's y is from the bottom, as clip space's
			return true;
		}

		// --- the lines ----------------------------------------------------------------
		struct Colour
		{
			float r, g, b, a;
		};
		constexpr Colour kWhite{ 1.0f, 1.0f, 1.0f, 0.85f };
		constexpr Colour kYellow{ 1.0f, 0.86f, 0.3f, 0.9f };
		constexpr Colour kOrange{ 1.0f, 0.55f, 0.15f, 0.9f };
		constexpr Colour kPink{ 1.0f, 0.6f, 1.0f, 0.9f };
		constexpr Colour kPurple{ 0.85f, 0.35f, 1.0f, 0.9f };
		constexpr Colour kGrey{ 0.6f, 0.6f, 0.6f, 0.5f };
		constexpr Colour kRed{ 1.0f, 0.3f, 0.3f, 0.9f };
		constexpr Colour kGreen{ 0.3f, 1.0f, 0.45f, 0.7f };
		constexpr Colour kViolet{ 0.8f, 0.45f, 1.0f, 0.8f };

		std::vector<Vertex> g_lines;

		void Line(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, const Colour& a_colour)
		{
			float x0, y0, x1, y1;
			if (ToScreen(a_from, x0, y0) && ToScreen(a_to, x1, y1) && g_lines.size() + 2 <= kMaxVertices) {
				g_lines.push_back({ x0, y0, a_colour.r, a_colour.g, a_colour.b, a_colour.a });
				g_lines.push_back({ x1, y1, a_colour.r, a_colour.g, a_colour.b, a_colour.a });
			}
		}

		// A small cross at a point, sized in pixels.
		void Mark(const RE::NiPoint3& a_at, const Colour& a_colour, float a_pixels)
		{
			float x, y;
			if (!ToScreen(a_at, x, y) || g_lines.size() + 4 > kMaxVertices) {
				return;
			}
			const float dx = a_pixels * 2.0f / g_width, dy = a_pixels * 2.0f / g_height;
			g_lines.push_back({ x - dx, y, a_colour.r, a_colour.g, a_colour.b, a_colour.a });
			g_lines.push_back({ x + dx, y, a_colour.r, a_colour.g, a_colour.b, a_colour.a });
			g_lines.push_back({ x, y - dy, a_colour.r, a_colour.g, a_colour.b, a_colour.a });
			g_lines.push_back({ x, y + dy, a_colour.r, a_colour.g, a_colour.b, a_colour.a });
		}

		void Circle(const RE::NiPoint3& a_centre, const RE::NiPoint3& a_u, const RE::NiPoint3& a_v, float a_radius, const Colour& a_colour)
		{
			constexpr int segments = 32;
			RE::NiPoint3  previous = a_centre + a_u * a_radius;
			for (int i = 1; i <= segments; ++i) {
				const float        angle = static_cast<float>(i) * (2.0f * kPi / segments);
				const RE::NiPoint3 point = a_centre + (a_u * std::cos(angle) + a_v * std::sin(angle)) * a_radius;
				Line(previous, point, a_colour);
				previous = point;
			}
		}

		void Gather(const collision::Frame& a_frame, const settings::Values& a_settings)
		{
			g_lines.clear();
			for (const auto& ray : a_frame.rays) {
				switch (ray.kind) {
				case collision::RayKind::Cast:
					if (a_settings.drawRays) {
						Line(ray.from, ray.to, kWhite);
						Mark(ray.hitAt, kWhite, 5.0f);
					}
					break;
				case collision::RayKind::Whisker:
					if (a_settings.drawWhiskers) {
						Line(ray.from, ray.hit ? ray.hitAt : ray.to, ray.hit ? (ray.counts ? kRed : kGreen) : kGreen);
						if (ray.hit) {
							Mark(ray.hitAt, ray.counts ? kRed : kGreen, 4.0f);
						}
					}
					break;
				case collision::RayKind::Disc:
					if (a_settings.drawRays) {
						const Colour colour = ray.forWhisker ? (ray.counts ? kPink : kPurple) : (ray.counts ? kYellow : kGrey);
						Line(ray.from, ray.hit ? ray.hitAt : ray.to, colour);
					}
					break;
				}
			}
			if (a_settings.drawDiscs) {
				for (const auto& disc : a_frame.discs) {
					const Colour colour = disc.forWhisker ? (disc.dropped ? kPurple : kPink) : (disc.dropped ? kOrange : kYellow);
					for (int r = 0; r < disc.rings; ++r) {
						for (int i = 0; i < 16; ++i) {
							Line(disc.ring[r][i], disc.ring[r][(i + 1) % 16], colour);
						}
					}
					for (int i = 0; i < disc.samples; ++i) {
						Mark(disc.point[i], disc.hit[i] ? colour : kGrey, disc.hit[i] ? 4.0f : 2.0f);
					}
				}
			}
			if (a_settings.drawBounds) {
				const RE::NiPoint3 pivot = collision::CastFrom();
				for (const auto& [centre, radius] : a_frame.bounds) {
					const RE::NiPoint3 axis = Normalized(centre - pivot);
					RE::NiPoint3       up{ 0.0f, 0.0f, 1.0f };
					if (std::fabs(axis.z) > 0.9f) {
						up = { 1.0f, 0.0f, 0.0f };
					}
					const RE::NiPoint3 u = Normalized(Cross(axis, up));
					const RE::NiPoint3 v = Normalized(Cross(axis, u));
					Circle(centre, u, v, radius, kViolet);
				}
			}
		}

		void Render(IDXGISwapChain* a_swapChain)
		{
			if (g_lines.empty()) {
				return;
			}
			auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
			if (!renderer) {
				return;
			}
			auto& data = renderer->GetRuntimeData();
			auto* device = reinterpret_cast<ID3D11Device*>(data.forwarder);
			auto* context = reinterpret_cast<ID3D11DeviceContext*>(data.context);
			if (!device || !context || !MakePipeline(device)) {
				return;
			}

			ComPtr<ID3D11Texture2D> backBuffer;
			if (FAILED(a_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
				return;
			}
			ComPtr<ID3D11RenderTargetView> target;
			if (FAILED(device->CreateRenderTargetView(backBuffer.Get(), nullptr, &target))) {
				return;
			}
			D3D11_TEXTURE2D_DESC description{};
			backBuffer->GetDesc(&description);

			D3D11_MAPPED_SUBRESOURCE mapped{};
			if (FAILED(context->Map(g_vertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
				return;
			}
			const auto count = static_cast<UINT>((std::min)(g_lines.size(), static_cast<std::size_t>(kMaxVertices)));
			std::memcpy(mapped.pData, g_lines.data(), count * sizeof(Vertex));
			context->Unmap(g_vertices.Get(), 0);

			// The state we touch, kept and put back: the game's own frame is
			// done, but the next one is not entitled to find it changed.
			ComPtr<ID3D11RenderTargetView> oldTarget;
			ComPtr<ID3D11DepthStencilView> oldDepth;
			context->OMGetRenderTargets(1, &oldTarget, &oldDepth);
			UINT           viewportCount = 1;
			D3D11_VIEWPORT oldViewport{};
			context->RSGetViewports(&viewportCount, &oldViewport);
			ComPtr<ID3D11BlendState> oldBlend;
			float                    oldBlendFactor[4]{};
			UINT                     oldSampleMask = 0;
			context->OMGetBlendState(&oldBlend, oldBlendFactor, &oldSampleMask);
			ComPtr<ID3D11DepthStencilState> oldDepthState;
			UINT                            oldStencilRef = 0;
			context->OMGetDepthStencilState(&oldDepthState, &oldStencilRef);
			ComPtr<ID3D11RasterizerState> oldRaster;
			context->RSGetState(&oldRaster);
			ComPtr<ID3D11InputLayout> oldLayout;
			context->IAGetInputLayout(&oldLayout);
			D3D11_PRIMITIVE_TOPOLOGY oldTopology{};
			context->IAGetPrimitiveTopology(&oldTopology);
			ComPtr<ID3D11VertexShader> oldVS;
			ComPtr<ID3D11PixelShader>  oldPS;
			context->VSGetShader(&oldVS, nullptr, nullptr);
			context->PSGetShader(&oldPS, nullptr, nullptr);
			ComPtr<ID3D11Buffer> oldVB;
			UINT                 oldStride = 0, oldOffset = 0;
			context->IAGetVertexBuffers(0, 1, &oldVB, &oldStride, &oldOffset);

			ID3D11RenderTargetView* targets[] = { target.Get() };
			context->OMSetRenderTargets(1, targets, nullptr);
			D3D11_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(description.Width), static_cast<float>(description.Height), 0.0f, 1.0f };
			context->RSSetViewports(1, &viewport);
			const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			context->OMSetBlendState(g_blend.Get(), blendFactor, 0xFFFFFFFF);
			context->OMSetDepthStencilState(g_noDepth.Get(), 0);
			context->RSSetState(g_raster.Get());
			context->IASetInputLayout(g_layout.Get());
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
			context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
			context->PSSetShader(g_pixelShader.Get(), nullptr, 0);
			context->GSSetShader(nullptr, nullptr, 0);
			context->HSSetShader(nullptr, nullptr, 0);
			context->DSSetShader(nullptr, nullptr, 0);
			ID3D11Buffer* buffers[] = { g_vertices.Get() };
			const UINT    stride = sizeof(Vertex), offset = 0;
			context->IASetVertexBuffers(0, 1, buffers, &stride, &offset);
			context->Draw(count, 0);

			ID3D11RenderTargetView* restoreTargets[] = { oldTarget.Get() };
			context->OMSetRenderTargets(1, restoreTargets, oldDepth.Get());
			context->RSSetViewports(viewportCount, &oldViewport);
			context->OMSetBlendState(oldBlend.Get(), oldBlendFactor, oldSampleMask);
			context->OMSetDepthStencilState(oldDepthState.Get(), oldStencilRef);
			context->RSSetState(oldRaster.Get());
			context->IASetInputLayout(oldLayout.Get());
			context->IASetPrimitiveTopology(oldTopology);
			context->VSSetShader(oldVS.Get(), nullptr, 0);
			context->PSSetShader(oldPS.Get(), nullptr, 0);
			ID3D11Buffer* restoreBuffers[] = { oldVB.Get() };
			context->IASetVertexBuffers(0, 1, restoreBuffers, &oldStride, &oldOffset);
		}

		HRESULT WINAPI PresentHook(IDXGISwapChain* a_swapChain, UINT a_syncInterval, UINT a_flags)
		{
			fade::Tick();

			// Nothing is drawn with a menu up: the pass runs after the UI, so
			// it would sit on top of the menu.
			const auto settings = settings::Current();
			auto*      ui = RE::UI::GetSingleton();
			const bool menuUp = ui && (ui->GameIsPaused() || ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) ||
										  ui->IsMenuOpen(RE::Console::MENU_NAME) || ui->numPausesGame > 0);
			if (!menuUp && (settings.drawDiscs || settings.drawWhiskers || settings.drawRays || settings.drawBounds)) {
				DXGI_SWAP_CHAIN_DESC description{};
				if (SUCCEEDED(a_swapChain->GetDesc(&description))) {
					g_width = static_cast<float>(description.BufferDesc.Width);
					g_height = static_cast<float>(description.BufferDesc.Height);
				}
				Gather(collision::LastFrame(), settings);
				Render(a_swapChain);
			}
			return g_originalPresent(a_swapChain, a_syncInterval, a_flags);
		}
	}

	bool Install()
	{
		auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
		if (!renderer) {
			spdlog::error("draw: no renderer yet");
			return false;
		}
		auto* swapChain = reinterpret_cast<IDXGISwapChain*>(renderer->GetRuntimeData().renderWindows[0].swapChain);
		if (!swapChain) {
			spdlog::error("draw: no swap chain yet");
			return false;
		}
		// IDXGISwapChain::Present is the ninth entry of its vtable.
		void** vtable = *reinterpret_cast<void***>(swapChain);
		void*  present = vtable[8];
		void*  trampoline = nullptr;
		if (const auto status = MH_CreateHook(present, reinterpret_cast<void*>(&PresentHook), &trampoline); status != MH_OK) {
			spdlog::error("draw: MH_CreateHook failed for Present: {}", MH_StatusToString(status));
			return false;
		}
		g_originalPresent = reinterpret_cast<PresentFn>(trampoline);
		if (const auto status = MH_EnableHook(present); status != MH_OK) {
			spdlog::error("draw: MH_EnableHook failed for Present: {}", MH_StatusToString(status));
			return false;
		}
		spdlog::info("draw: hooked the swap chain's Present");
		return true;
	}
}
