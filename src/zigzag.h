#ifndef ZIGZAG_H_INCLUDED
#define ZIGZAG_H_INCLUDED

template <int rows, int cols>
class ZigZag
{
public:
    enum:int
    {
        n=rows*cols
    };

    int table[n];

    ZigZag()
    {
        int cur_x=0,cur_y=0;
        int dx=1,dy=-1; // in upper-right direction initially
        for (int i=0;i<n;i++)
        {
            table[i]=cur_y*cols+cur_x;
            if (out_of_map<rows,cols>(cur_x+dx,cur_y+dy))
            {
                if (cur_x<cols-1 && cur_y<rows-1)
                {
                    if (dx>0) cur_x++;else cur_y++;
                }else
                {
                    if (dx<0) cur_x++;else cur_y++;
                }
                // change direction
                dx=-dx;dy=-dy;
            }else
            {
                cur_x+=dx;
                cur_y+=dy;
            }
        }
        assert(table[n-1]==n-1);
    }

    int operator [](int transformed_index) const
    {
        return table[transformed_index];
    }

    int operator ()(int transformed_x, int transformed_y) const
    {
        return table[transformed_y*cols+transformed_x];
    }
};

#endif // ZIGZAG_H_INCLUDED
