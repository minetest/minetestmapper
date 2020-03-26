/*
 * =====================================================================
 *        Version:  1.0
 *        Created:  25.08.2012 10:55:27
 *         Author:  Miroslav Bendík
 *        Company:  LinuxOS.sk
 * =====================================================================
 */

#include "PixelAttributes.h"

using namespace std;

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
	*m_pixelAttributes[FirstLine] = *m_pixelAttributes[LastLine];
	for (size_t i = 1; i < LineCount - 1; ++i) {
		*m_pixelAttributes[i] = *m_pixelAttributes[EmptyLine];
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

