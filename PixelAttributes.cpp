#include <cstring>

#include "PixelAttributes.h"

PixelAttributes::PixelAttributes():
	m_width(0)
{
	for (size_t i = 0; i < LineCount; ++i) {
		m_pixelAttributes[i] = nullptr;
	}
}

PixelAttributes::~PixelAttributes()
{
	freeAttributes();
}

void PixelAttributes::setWidth(int width)
{
	freeAttributes();
	m_width = width + 1; // 1px gradient calculation
	for (size_t i = 0; i < LineCount; ++i) {
		m_pixelAttributes[i] = new PixelAttribute[m_width];
	}
}

void PixelAttributes::scroll()
{
	size_t lineLength = m_width * sizeof(PixelAttribute);
	memcpy(m_pixelAttributes[FirstLine], m_pixelAttributes[LastLine], lineLength);
	for (size_t i = 1; i < LineCount - 1; ++i) {
		memcpy(m_pixelAttributes[i], m_pixelAttributes[EmptyLine], lineLength);
	}
}

void PixelAttributes::freeAttributes()
{
	for (size_t i = 0; i < LineCount; ++i) {
		if (m_pixelAttributes[i] != nullptr) {
			delete[] m_pixelAttributes[i];
			m_pixelAttributes[i] = nullptr;
		}
	}
}

