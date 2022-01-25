#include <stdexcept>

#include <utf8.h>

#include "D2DRenderer.hpp"

D2DRenderer::D2DRenderer(ID3D11Device* device, IDXGISurface* surface) {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2d1.GetAddressOf()))) {
        throw std::runtime_error{"Failed to create D2D factory"};
    }

    ComPtr<IDXGIDevice> dxgi_device{};

    if (FAILED(device->QueryInterface(dxgi_device.GetAddressOf()))) {
        throw std::runtime_error{"Failed to query DXGI device"};
    }

    if (FAILED(m_d2d1->CreateDevice(dxgi_device.Get(), m_device.GetAddressOf()))) {
        throw std::runtime_error{"Failed to create D2D device"};
    }

    if (FAILED(m_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, m_context.GetAddressOf()))) {
        throw std::runtime_error{"Failed to create D2D device context"};
    }

    float dpi_x{};
    float dpi_y{};

    m_d2d1->GetDesktopDpi(&dpi_x, &dpi_y);

    auto bitmap_props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi_x, dpi_y);

    if (FAILED(m_context->CreateBitmapFromDxgiSurface(surface, &bitmap_props, &m_rt))) {
        throw std::runtime_error{"Failed to create D2D render target"};
    }

    if (FAILED(m_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_brush))) {
        throw std::runtime_error{"Failed to create D2D brush"};
    }

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &m_dwrite))) {
        throw std::runtime_error{"Failed to create DWrite factory"};
    }
}

void D2DRenderer::begin() {
    m_context->SetTarget(m_rt.Get());
    m_context->BeginDraw();
    m_context->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));
}

void D2DRenderer::end() {
    m_context->EndDraw();
}

int D2DRenderer::create_font(std::string name, int size, bool bold, bool italic) {
    // Look for a font already matching the description.
    for (int i = 0; i < m_fonts.size(); i++) {
        if (m_fonts[i].name == name && m_fonts[i].size == size && m_fonts[i].bold == bold && m_fonts[i].italic == italic) {
            return i;
        }
    }

    // Create a new font.
    Font font{};
    font.name = std::move(name);
    font.size = size;
    font.bold = bold;
    font.italic = italic;

    std::wstring wide_name{};
    utf8::utf8to16(font.name.begin(), font.name.end(), std::back_inserter(wide_name));

    if (FAILED(m_dwrite->CreateTextFormat(wide_name.c_str(), nullptr, bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
            italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &font.text_format))) {
        throw std::runtime_error{"Failed to create DWrite text format"};
    }

    m_fonts.emplace_back(std::move(font));

    return m_fonts.size() - 1;
}

void D2DRenderer::color(float r, float g, float b, float a) {
    m_brush->SetColor(D2D1::ColorF(r, g, b, a));
}

void D2DRenderer::text(int font, float x, float y, const std::string& text) {
    if (font < 0 || font >= m_fonts.size()) {
        throw std::runtime_error{"Invalid font"};
    }

    static std::wstring wide_text{};
    wide_text.clear();
    utf8::utf8to16(text.begin(), text.end(), std::back_inserter(wide_text));

    m_context->DrawText(
        wide_text.c_str(), wide_text.size(), m_fonts[font].text_format.Get(), {x, y, 10000.0f, 10000.0f}, m_brush.Get());
}

void D2DRenderer::fill_rect(float x, float y, float w, float h) {
    m_context->FillRectangle({x, y, x + w, y + h}, m_brush.Get());
}

void D2DRenderer::outline_rect(float x, float y, float w, float h, float thickness) {
    m_context->DrawRectangle({x, y, x + w, y + h}, m_brush.Get(), thickness);
}
