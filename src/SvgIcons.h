#ifndef SVG_ICONS_H
#define SVG_ICONS_H

#include <QString>
#include <QMap>

namespace ArcMeta {

class SvgIcons {
public:
    static inline const QMap<QString, QString> icons = {
        {"close", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z'/></svg>"},
        {"maximize", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M4,4H20V20H4V4M6,8V18H18V8H6Z'/></svg>"},
        {"restore", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M4,8H8V4H20V16H16V20H4V8M16,8V14H18V6H10V8H16M6,12V18H14V12H6Z'/></svg>"},
        {"minimize", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M20,14H4V10H20V14Z'/></svg>"},
        {"pin", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M16,12V4H17V2H7V4H8V12L6,14V16H11.2V22L12,22.8L12.8,22V16H18V14L16,12Z'/></svg>"},
        {"arrow_left", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M20,11V13H8L13.5,18.5L12.08,19.92L4.16,12L12.08,4.08L13.5,5.5L8,11H20Z'/></svg>"},
        {"arrow_right", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M4,11V13H16L10.5,18.5L11.92,19.92L19.84,12L11.92,4.08L10.5,5.5L16,11H4Z'/></svg>"},
        {"arrow_up", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M13,20V8L18.5,13.5L19.92,12.08L12,4.16L4.08,12.08L5.5,13.5L11,8V20H13Z'/></svg>"},
        {"star", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M12,17.27L18.18,21L16.54,13.97L22,9.24L14.81,8.62L12,2L9.19,8.62L2,9.24L7.45,13.97L5.82,21L12,17.27Z'/></svg>"},
        {"star_outline", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M12,15.39L8.24,17.66L9.23,13.38L5.91,10.5L10.29,10.13L12,6.09L13.71,10.13L18.09,10.5L14.77,13.38L15.76,17.66L12,15.39M22,9.24L14.81,8.62L12,2L9.19,8.62L2,9.24L7.45,13.97L5.82,21L12,17.27L18.18,21L16.54,13.97L22,9.24Z'/></svg>"},
        {"encrypt", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M12,17A2,2 0 0,0 14,15C14,13.89 13.11,13 12,13A2,2 0 0,0 10,15C10,16.11 10.89,17 12,17M18,8A2,2 0 0,1 20,10V20A2,2 0 0,1 18,22H6A2,2 0 0,1 4,20V10C4,8.89 4.9,8 6,8H7V6A5,5 0 0,1 12,1A5,5 0 0,1 17,6V8H18M12,3A3,3 0 0,0 9,6V8H15V6A3,3 0 0,0 12,3Z'/></svg>"},
        {"lock", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M12,17A2,2 0 0,0 14,15C14,13.89 13.11,13 12,13A2,2 0 0,0 10,15C10,16.11 10.89,17 12,17M18,8H17V6A5,5 0 0,0 12,1A5,5 0 0,0 7,6V8H6A2,2 0 0,0 4,10V20A2,2 0 0,0 6,22H18A2,2 0 0,0 20,20V10A2,2 0 0,0 18,8M9,6A3,3 0 0,1 12,3A3,3 0 0,1 15,6V8H9V6Z'/></svg>"},
        {"folder", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M10,4H4C2.89,4 2,4.89 2,6V18A2,2 0 0,0 4,20H20A2,2 0 0,0 22,18V8C22,6.89 21.1,6 20,6H12L10,4Z'/></svg>"},
        {"file", "<svg viewBox='0 0 24 24'><path fill='currentColor' d='M13,9V3.5L18.5,9M6,2C4.89,2 4,2.89 4,4V20A2,2 0 0,0 6,22H18A2,2 0 0,0 20,20V8L14,2H6Z'/></svg>"}
    };
};

} // namespace ArcMeta

#endif // SVG_ICONS_H
