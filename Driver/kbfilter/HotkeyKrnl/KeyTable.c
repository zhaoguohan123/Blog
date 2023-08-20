#include <ntddk.h>
#include "ioctlproto.h"

// Copy from WinUser.h
#define VK_SPACE          0x20
#define VK_NUMPAD0        0x60
#define VK_NUMPAD1        0x61
#define VK_NUMPAD2        0x62
#define VK_NUMPAD3        0x63
#define VK_NUMPAD4        0x64
#define VK_NUMPAD5        0x65
#define VK_NUMPAD6        0x66
#define VK_NUMPAD7        0x67
#define VK_NUMPAD8        0x68
#define VK_NUMPAD9        0x69
#define VK_MULTIPLY       0x6A
#define VK_ADD            0x6B
#define VK_SEPARATOR      0x6C
#define VK_SUBTRACT       0x6D
#define VK_DECIMAL        0x6E
#define VK_DIVIDE         0x6F
#define VK_F1             0x70
#define VK_F2             0x71
#define VK_F3             0x72
#define VK_F4             0x73
#define VK_F5             0x74
#define VK_F6             0x75
#define VK_F7             0x76
#define VK_F8             0x77
#define VK_F9             0x78
#define VK_F10            0x79
#define VK_F11            0x7A

typedef struct _KEY_TABLE {
    UINT    VirtualkKey;
    USHORT  MakeCode;
} KEY_TABLE, *PKEY_TABLE;

KEY_TABLE KeyTab[] =
{
    'A', 0x1E,
    'B', 0x30,
    'C', 0x2E,
    'D', 0x20,
    'E', 0x12,
    'F', 0x21,
    'G', 0x22,
    'H', 0x23,
    'I', 0x17,
    'J', 0x24,
    'K', 0x25,
    'L', 0x26,
    'M', 0x32,
    'N', 0x31,
    'O', 0x18,
    'P', 0x19,
    'Q', 0x10,
    'R', 0x13,
    'S', 0x1F,
    'T', 0x14,
    'U', 0x16,
    'V', 0x2F,
    'W', 0x11,
    'X', 0x2D,
    'Y', 0x15,
    'Z', 0x2C,
    '0', 0x0B,
    '1', 0x02,
    '2', 0x03,
    '3', 0x04,
    '4', 0x05,
    '5', 0x06,
    '6', 0x07,
    '7', 0x08,
    '8', 0x09,
    '9', 0x0A,
    VK_NUMPAD0, 0x52,
    VK_NUMPAD1, 0x4F,
    VK_NUMPAD2, 0x50,
    VK_NUMPAD3, 0x51,
    VK_NUMPAD4, 0x4B,
    VK_NUMPAD5, 0x4C,
    VK_NUMPAD6, 0x4D,
    VK_NUMPAD7, 0x47,
    VK_NUMPAD8, 0x48,
    VK_NUMPAD9, 0x49,
    VK_MULTIPLY, 0x37,
    VK_ADD, 0x4E,
    VK_SUBTRACT, 0x4A,
    VK_DECIMAL, 0x53,
    VK_DIVIDE, 0x35,
    VK_F1, 0x3B,
    VK_F2, 0x3C,
    VK_F3, 0x3D,
    VK_F4, 0x3E,
    VK_F5, 0x3F,
    VK_F6, 0x40,
    VK_F7, 0x41,
    VK_F8, 0x42,
    VK_F9, 0x43,
    VK_F10, 0x44,
    VK_F11, 0x57,
    VK_SPACE, 0x39
};