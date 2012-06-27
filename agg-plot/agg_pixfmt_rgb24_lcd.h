//----------------------------------------------------------------------------
// Anti-Grain Geometry (AGG) - Version 2.5
// A high quality rendering engine for C++
// Copyright (C) 2002-2006 Maxim Shemanarev
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://antigrain.com
//
// AGG is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// AGG is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with AGG; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
// MA 02110-1301, USA.
//----------------------------------------------------------------------------

#ifndef AGG_PIXFMT_RGB24_LCD_INCLUDED
#define AGG_PIXFMT_RGB24_LCD_INCLUDED

#include <string.h>
#include <stdlib.h>

#include "agg_basics.h"
#include "agg_color_rgba.h"
#include "agg_rendering_buffer.h"

namespace agg
{


    //=====================================================lcd_distribution_lut
    class lcd_distribution_lut
    {
    public:
        lcd_distribution_lut(double prim, double second, double tert)
        {
            double norm = 1.0 / (prim + second*2 + tert*2);
            prim   *= norm;
            second *= norm;
            tert   *= norm;
            for(unsigned i = 0; i < 256; i++)
            {
	      unsigned char s = round(second * i);
	      unsigned char t = round(tert   * i);
	      unsigned char p = i - (2*s + 2*t);

	      m_data[3*i + 1] = s; /* secondary */
	      m_data[3*i + 2] = t; /* tertiary */
	      m_data[3*i    ] = p; /* primary */
            }
        }

      unsigned convolution(const int8u* covers, int i0, int i_min, int i_max) const
      {
	unsigned sum = 0;
	int k_min = (i0 >= i_min + 2 ? -2 : i_min - i0);
	int k_max = (i0 <= i_max - 2 ?  2 : i_max - i0);
	for (int k = k_min; k <= k_max; k++)
	  {
	    /* select the primary, secondary or tertiary channel */
	    int channel = abs(k) % 3;
	    int8u c = covers[i0 + k];
	    sum += m_data[3*c + channel];
	  }

	return sum;
      }

    private:
	unsigned char m_data[256*3];
    };





    //========================================================pixfmt_rgb24_lcd
    class pixfmt_rgb24_lcd
    {
    public:
        typedef rgba8 color_type;
        typedef rendering_buffer::row_data row_data;
        typedef color_type::value_type value_type;
        typedef color_type::calc_type calc_type;

        //--------------------------------------------------------------------
        pixfmt_rgb24_lcd(rendering_buffer& rb, const lcd_distribution_lut& lut)
            : m_rbuf(&rb),
              m_lut(&lut)
        {
        }

        //--------------------------------------------------------------------
        unsigned width()  const { return m_rbuf->width() * 3;  }
        unsigned height() const { return m_rbuf->height(); }


        //--------------------------------------------------------------------
        void blend_hline(int x, int y,
                         unsigned len,
                         const color_type& c,
                         int8u cover)
        {
            int8u* p = m_rbuf->row_ptr(y) + x + x + x;
            int alpha = int(cover) * c.a;
            do
            {
                p[0] = (int8u)((((c.r - p[0]) * alpha) + (p[0] << 16)) >> 16);
                p[1] = (int8u)((((c.g - p[1]) * alpha) + (p[1] << 16)) >> 16);
                p[2] = (int8u)((((c.b - p[2]) * alpha) + (p[2] << 16)) >> 16);
                p += 3;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_solid_hspan(int x, int y,
                               unsigned len,
                               const color_type& c,
                               const int8u* covers)
        {
	  unsigned rowlen = m_rbuf->width();
	  int cx = (x - 2 >= 0 ? -2 : -x);
	  int cx_max = (len + 2 <= rowlen ? len + 1 : rowlen - 1);

	  int i = (x + cx) % 3;

	  int8u rgb[3] = { c.r, c.g, c.b };
	  int8u* p = m_rbuf->row_ptr(y) + (x + cx);

	  for (/* */; cx <= cx_max; cx++)
	    {
	      int alpha = m_lut->convolution(covers, cx, 0, len - 1) * c.a;
	      if (alpha)
		{
		  if (alpha == 255*255)
		    {
		      *p = (int8u)rgb[i];
		    }
		  else
		    {
		      *p = (int8u)((((rgb[i] - *p) * alpha) + (*p << 16)) >> 16);
		    }
		}

	      p ++;
	      i = (i + 1) % 3;
	    }
	}

    private:
        rendering_buffer* m_rbuf;
        const lcd_distribution_lut* m_lut;
    };


}

#endif

