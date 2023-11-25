/*
   Copyright (C) 2009 Red Hat, Inc.

   This software is licensed under the GNU General Public License,
   version 2 (GPLv2) (see COPYING for details), subject to the
   following clarification.

   With respect to binaries built using the Microsoft(R) Windows
   Driver Kit (WDK), GPLv2 does not extend to any code contained in or
   derived from the WDK ("WDK Code").  As to WDK Code, by using or
   distributing such binaries you agree to be bound by the Microsoft
   Software License Terms for the WDK.  All WDK Code is considered by
   the GPLv2 licensors to qualify for the special exception stated in
   section 3 of GPLv2 (commonly known as the system library
   exception).

   There is NO WARRANTY for this software, express or implied,
   including the implied warranties of NON-INFRINGEMENT, TITLE,
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#include "os_dep.h"
#include "qxldd.h"
#include "utils.h"
#include "res.h"
#include "rop.h"
#include "surface.h"


enum ROP3type {
    ROP3_TYPE_ERR,
    ROP3_TYPE_FILL,
    ROP3_TYPE_OPAQUE,
    ROP3_TYPE_COPY,
    ROP3_TYPE_BLEND,
    ROP3_TYPE_BLACKNESS,
    ROP3_TYPE_WHITENESS,
    ROP3_TYPE_INVERS,
    ROP3_TYPE_ROP3,
    ROP3_TYPE_NOP,
};


ROP3Info rops2[] = {
    {QXL_EFFECT_OPAQUE, 0, ROP3_TYPE_BLACKNESS, SPICE_ROPD_OP_BLACKNESS},                     //0
    {QXL_EFFECT_BLEND, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_OR |
                                                               SPICE_ROPD_INVERS_RES},        //DPon
    {QXL_EFFECT_NOP_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL,
                                               SPICE_ROPD_INVERS_BRUSH | SPICE_ROPD_OP_AND},  //DPna
    {QXL_EFFECT_OPAQUE, ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_INVERS_BRUSH |
                                                    SPICE_ROPD_OP_PUT},                       //Pn
    {QXL_EFFECT_BLACKNESS_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL,
                                              SPICE_ROPD_INVERS_DEST | SPICE_ROPD_OP_AND},    //PDna
    {QXL_EFFECT_REVERT_ON_DUP, ROP3_DEST, ROP3_TYPE_INVERS, SPICE_ROPD_OP_INVERS},            //Dn
    {QXL_EFFECT_REVERT_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_XOR},    //DPx
    {QXL_EFFECT_BLACKNESS_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL,
                                              SPICE_ROPD_OP_AND | SPICE_ROPD_INVERS_RES},     //DPan
    {QXL_EFFECT_NOP_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_AND},       //DPa
    {QXL_EFFECT_BLEND, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_XOR |
                                               SPICE_ROPD_INVERS_RES},                        //DPxn
    {QXL_EFFECT_NOP, ROP3_DEST, ROP3_TYPE_NOP, 0},                                            //D
    {QXL_EFFECT_NOP_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL,
                                            SPICE_ROPD_INVERS_BRUSH | SPICE_ROPD_OP_OR},      //DPno
    {QXL_EFFECT_OPAQUE, ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_PUT},                       //P
    {QXL_EFFECT_BLEND, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_INVERS_DEST |
                                               SPICE_ROPD_OP_OR},                             //PDno
    {QXL_EFFECT_NOP_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_OR},        //DPo
    {QXL_EFFECT_OPAQUE, 0, ROP3_TYPE_WHITENESS, SPICE_ROPD_OP_WHITENESS},                     //1
};


ROP3Info rops3[] = {

    //todo: update rop3 effect

    {QXL_EFFECT_OPAQUE, 0, ROP3_TYPE_BLACKNESS, 0},                                 //0
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x01},                             //DPSoon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x02},                             //DPSona
                                                                                    //PSon
    {QXL_EFFECT_OPAQUE, ROP3_SRC | ROP3_BRUSH, ROP3_TYPE_OPAQUE, SPICE_ROPD_OP_OR |
                                               SPICE_ROPD_INVERS_RES},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x04},                             //SDPona
                                                                                    //DPon
    {QXL_EFFECT_BLEND, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_OR |
                                               SPICE_ROPD_INVERS_RES},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x06},                             //PDSxnon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x07},                             //PDSaon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x08},                             //SDPnaa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x09},                             //PDSxon
                                                                                    //DPna
    {QXL_EFFECT_NOP_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL,
                                                                   SPICE_ROPD_INVERS_BRUSH |
                                                                   SPICE_ROPD_OP_AND},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x0b},                             //PSDnaon
                                                                                    //SPna
    {QXL_EFFECT_OPAQUE, ROP3_SRC | ROP3_BRUSH, ROP3_TYPE_OPAQUE, SPICE_ROPD_INVERS_BRUSH |
                                                                 SPICE_ROPD_OP_AND },
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x0d},                             //PDSnaon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x0e},                             //PDSonon
                                                                                    //Pn
    {QXL_EFFECT_OPAQUE, ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_INVERS_BRUSH | SPICE_ROPD_OP_PUT},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x10},                             //PDSona
                                                                                    //DSon
    {QXL_EFFECT_BLEND, ROP3_SRC | ROP3_DEST, ROP3_TYPE_BLEND, SPICE_ROPD_OP_OR |
                                             SPICE_ROPD_INVERS_RES},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x12},                             //SDPxnon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x13},                             //SDPaon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x14},                             //DPSxnon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x15},                             //DPSaon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x16},                             //PSDPSanaxx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x17},                             //SSPxDSxaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x18},                             //SPxPDxa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x19},                             //SDPSanaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x1a},                             //PDSPaox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x1b},                             //SDPSxaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x1c},                             //PSDPaox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x1d},                             //DSPDxaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x1e},                             //PDSox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x1f},                             //PDSoan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x20},                             //DPSnaa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x21},                             //SDPxon
                                                                                    //DSna
    {QXL_EFFECT_NOP_ON_DUP, ROP3_SRC | ROP3_DEST, ROP3_TYPE_BLEND, SPICE_ROPD_INVERS_SRC |
                                                                   SPICE_ROPD_OP_AND},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x23},                             //SPDnaon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x24},                             //SPxDSxa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x25},                             //PDSPanaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x26},                             //SDPSaox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x27},                             //SDPSxnox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x28},                             //DPSxa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x29},                             //PSDPSaoxxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x2a},                             //DPSana
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x2b},                             //SSPxPDxaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x2c},                             //SPDSoax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x2d},                             //PSDnox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x2e},                             //PSDPxox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x2f},                             //PSDnoan
                                                                                    //PSna
    {QXL_EFFECT_OPAQUE, ROP3_SRC | ROP3_BRUSH, ROP3_TYPE_OPAQUE, SPICE_ROPD_INVERS_SRC |
                                                                 SPICE_ROPD_OP_AND},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x31},                             //SDPnaon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x32},                             //SDPSoox
    {QXL_EFFECT_OPAQUE, ROP3_SRC, ROP3_TYPE_COPY, SPICE_ROPD_INVERS_SRC |
                                                  SPICE_ROPD_OP_PUT},               //Sn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x34},                             //SPDSaox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x35},                             //SPDSxnox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x36},                             //SDPox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x37},                             //SDPoan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x38},                             //PSDPoax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x39},                             //SPDnox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x3a},                             //SPDSxox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x3b},                             //SPDnoan
    {QXL_EFFECT_OPAQUE, ROP3_SRC | ROP3_BRUSH, ROP3_TYPE_OPAQUE, SPICE_ROPD_OP_XOR},//PSx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x3d},                             //SPDSonox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x3e},                             //SPDSnaox
                                                                                    //PSan
    {QXL_EFFECT_OPAQUE, ROP3_SRC | ROP3_BRUSH, ROP3_TYPE_OPAQUE, SPICE_ROPD_OP_AND |
                                                                 SPICE_ROPD_INVERS_RES},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x40},                             //PSDnaa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x41},                             //DPSxon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x42},                             //SDxPDxa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x43},                             //SPDSanaxn
                                                                                    //SDna
    {QXL_EFFECT_BLEND, ROP3_SRC | ROP3_DEST, ROP3_TYPE_BLEND, SPICE_ROPD_INVERS_DEST |
                                                              SPICE_ROPD_OP_AND},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x45},                             //DPSnaon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x46},                             //DSPDaox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x47},                             //PSDPxaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x48},                             //SDPxa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x49},                             //PDSPDaoxxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x4a},                             //DPSDoax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x4b},                             //PDSnox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x4c},                             //SDPana
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x4d},                             //SSPxDSxoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x4e},                             //PDSPxox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x4f},                             //PDSnoan
                                                                                    //PDna
    {QXL_EFFECT_BLEND, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_INVERS_DEST |
                                                               SPICE_ROPD_OP_AND},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x51},                             //DSPnaon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x52},                             //DPSDaox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x53},                             //SPDSxaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x54},                             //DPSonon
    {QXL_EFFECT_NOP_ON_DUP, ROP3_DEST, ROP3_TYPE_INVERS, 0},                        //Dn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x56},                             //DPSox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x57},                             //DPSoan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x58},                             //PDSPoax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x59},                             //DPSnox
    {QXL_EFFECT_REVERT_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_XOR},
                                                                                    //DPx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x5b},                             //DPSDonox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x5c},                             //DPSDxox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x5d},                             //DPSnoan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x5e},                             //DPSDnaox
                                                                                    //DPan
    {QXL_EFFECT_BLEND, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_AND |
                                                               SPICE_ROPD_INVERS_RES},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x60},                             //PDSxa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x61},                             //DSPDSaoxxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x62},                             //DSPDoax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x63},                             //SDPnox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x64},                             //SDPSoax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x65},                             //DSPnox
    {QXL_EFFECT_REVERT_ON_DUP, ROP3_SRC | ROP3_DEST, ROP3_TYPE_BLEND,
                                                     SPICE_ROPD_OP_XOR},            //DSx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x67},                             //SDPSonox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x68},                             //DSPDSonoxxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x69},                             //PDSxxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x6a},                             //DPSax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x6b},                             //PSDPSoaxxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x6c},                             //SDPax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x6d},                             //PDSPDoaxxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x6e},                             //SDPSnoax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x6f},                             //PDSxnan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x70},                             //PDSana
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x71},                             //SSDxPDxaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x72},                             //SDPSxox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x73},                             //SDPnoan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x74},                             //DSPDxox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x75},                             //DSPnoan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x76},                             //SDPSnaox
                                                                                    //DSan
    {QXL_EFFECT_BLEND, ROP3_SRC | ROP3_DEST, ROP3_TYPE_BLEND, SPICE_ROPD_OP_AND |
                                                              SPICE_ROPD_INVERS_RES},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x78},                             //PDSax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x79},                             //DSPDSoaxxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x7a},                             //DPSDnoax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x7b},                             //SDPxnan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x7c},                             //SPDSnoax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x7d},                             //DPSxnan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x7e},                             //SPxDSxo
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x7f},                             //DPSaan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x80},                             //DPSaa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x81},                             //SPxDSxon
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x82},                             //DPSxna
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x83},                             //SPDSnoaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x84},                             //SDPxna
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x85},                             //PDSPnoaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x86},                             //DSPDSoaxx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x87},                             //PDSaxn
    {QXL_EFFECT_NOP_ON_DUP, ROP3_SRC | ROP3_DEST, ROP3_TYPE_BLEND,
                                                  SPICE_ROPD_OP_AND},               //DSa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x89},                             //SDPSnaoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x8a},                             //DSPnoa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x8b},                             //DSPDxoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x8c},                             //SDPnoa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x8d},                             //SDPSxoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x8e},                             //SSDxPDxax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x8f},                             //PDSanan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x90},                             //PDSxna
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x91},                             //SDPSnoaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x92},                             //DPSDPoaxx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x93},                             //SPDaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x94},                             //PSDPSoaxx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x95},                             //DPSaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x96},                             //DPSxx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x97},                             //PSDPSonoxx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x98},                             //SDPSonoxn
                                                                                    //DSxn
    {QXL_EFFECT_BLEND, ROP3_SRC | ROP3_DEST, ROP3_TYPE_BLEND, SPICE_ROPD_OP_XOR |
                                                              SPICE_ROPD_INVERS_RES},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x9a},                             //DPSnax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x9b},                             //SDPSoaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x9c},                             //SPDnax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x9d},                             //DSPDoaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x9e},                             //DSPDSaoxx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0x9f},                             //PDSxan
    {QXL_EFFECT_NOP_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL,
                                                    SPICE_ROPD_OP_AND},             //DPa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xa1},                             //PDSPnaoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xa2},                             //DPSnoa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xa3},                             //DPSDxoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xa4},                             //PDSPonoxn
                                                                                    //PDxn
    {QXL_EFFECT_BLEND, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_XOR |
                                                               SPICE_ROPD_INVERS_RES},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xa6},                             //DSPnax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xa7},                             //PDSPoaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xa8},                             //DPSoa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xa9},                             //DPSoxn
    {QXL_EFFECT_NOP, ROP3_DEST, ROP3_TYPE_NOP, 0},                                  //D
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xab},                             //DPSono
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xac},                             //SPDSxax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xad},                             //DPSDaoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xae},                             //DSPnao
                                                                                    //DPno
    {QXL_EFFECT_NOP_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL,
                                                    SPICE_ROPD_INVERS_BRUSH | SPICE_ROPD_OP_OR},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xb0},                             //PDSnoa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xb1},                             //PDSPxoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xb2},                             //SSPxDSxox
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xb3},                             //SDPanan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xb4},                             //PSDnax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xb5},                             //DPSDoaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xb6},                             //DPSDPaoxx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xb7},                             //SDPxan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xb8},                             //PSDPxax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xb9},                             //DSPDaoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xba},                             //DPSnao
                                                                                    //DSno
    {QXL_EFFECT_NOP_ON_DUP, ROP3_SRC | ROP3_DEST, ROP3_TYPE_BLEND,
                                                       SPICE_ROPD_INVERS_SRC | SPICE_ROPD_OP_OR},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xbc},                             //SPDSanax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xbd},                             //SDxPDxan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xbe},                             //DPSxo
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xbf},                             //DPSano
    {QXL_EFFECT_OPAQUE, ROP3_SRC | ROP3_BRUSH, ROP3_TYPE_OPAQUE, SPICE_ROPD_OP_AND},//PSa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xc1},                             //SPDSnaoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xc2},                             //SPDSonoxn
                                                                                    //PSxn
    {QXL_EFFECT_OPAQUE, ROP3_SRC | ROP3_BRUSH, ROP3_TYPE_OPAQUE, SPICE_ROPD_OP_XOR |
                                                                 SPICE_ROPD_INVERS_RES},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xc4},                             //SPDnoa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xc5},                             //SPDSxoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xc6},                             //SDPnax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xc7},                             //PSDPoaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xc8},                             //SDPoa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xc9},                             //SPDoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xca},                             //DPSDxax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xcb},                             //SPDSaoxn
    {QXL_EFFECT_OPAQUE, ROP3_SRC, ROP3_TYPE_COPY, SPICE_ROPD_OP_PUT},                     //S
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xcd},                             //SDPono
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xce},                             //SDPnao
                                                                                    //SPno
    {QXL_EFFECT_OPAQUE, ROP3_SRC | ROP3_BRUSH, ROP3_TYPE_OPAQUE,
                                               SPICE_ROPD_INVERS_BRUSH |
                                               SPICE_ROPD_OP_OR},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xd0},                             //PSDnoa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xd1},                             //PSDPxoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xd2},                             //PDSnax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xd3},                             //SPDSoaxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xd4},                             //SSPxPDxax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xd5},                             //DPSanan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xd6},                             //PSDPSaoxx
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xd7},                             //DPSxan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xd8},                             //PDSPxax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xd9},                             //SDPSaoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xda},                             //DPSDanax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xdb},                             //SPxDSxan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xdc},                             //SPDnao
                                                                                    //SDno
    {QXL_EFFECT_BLEND, ROP3_SRC | ROP3_DEST, ROP3_TYPE_BLEND,
                                             SPICE_ROPD_INVERS_DEST |
                                             SPICE_ROPD_OP_OR},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xde},                             //SDPxo
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xdf},                             //SDPano
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xe0},                             //PDSoa
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xe1},                             //PDSoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xe2},                             //DSPDxax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xe3},                             //PSDPaoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xe4},                             //SDPSxax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xe5},                             //PDSPaoxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xe6},                             //SDPSanax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xe7},                             //SPxPDxan
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xe8},                             //SSPxDSxax
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xe9},                             //DSPDSanaxxn
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xea},                             //DPSao
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xeb},                             //DPSxno
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xec},                             //SDPao
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xed},                             //SDPxno
    {QXL_EFFECT_NOP_ON_DUP, ROP3_SRC | ROP3_DEST, ROP3_TYPE_BLEND,
                                                  SPICE_ROPD_OP_OR},                //DSo
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xef},                             //SDPnoo
    {QXL_EFFECT_OPAQUE, ROP3_BRUSH, ROP3_TYPE_FILL, SPICE_ROPD_OP_PUT},                   //P
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xf1},                             //PDSono
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xf2},                             //PDSnao
                                                                                    //PSno
    {QXL_EFFECT_OPAQUE, ROP3_SRC | ROP3_BRUSH, ROP3_TYPE_OPAQUE,
                                               SPICE_ROPD_INVERS_SRC |
                                               SPICE_ROPD_OP_OR},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xf4},                             //PSDnao
                                                                                    //PDno
    {QXL_EFFECT_BLEND, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL,
                                               SPICE_ROPD_INVERS_DEST |
                                               SPICE_ROPD_OP_OR},
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xf6},                             //PDSxo
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xf7},                             //PDSano
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xf8},                             //PDSao
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xf9},                             //PDSxno
    {QXL_EFFECT_NOP_ON_DUP, ROP3_DEST | ROP3_BRUSH, ROP3_TYPE_FILL,
                                                    SPICE_ROPD_OP_OR},              //DPo
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xfb},                             //DPSnoo
    {QXL_EFFECT_OPAQUE, ROP3_SRC | ROP3_BRUSH, ROP3_TYPE_OPAQUE, SPICE_ROPD_OP_OR}, //PSo
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xfd},                             //PSDnoo
    {QXL_EFFECT_BLEND, ROP3_ALL, ROP3_TYPE_ROP3, 0xfe},                             //DPSoo
    {QXL_EFFECT_OPAQUE, 0, ROP3_TYPE_WHITENESS, 1},                                 //1
};


static BOOL DoFill(PDev *pdev, UINT32 surface_id, RECTL *area, CLIPOBJ *clip, BRUSHOBJ *brush,
                   POINTL *brush_pos, ROP3Info *rop_info, SURFOBJ *mask, POINTL *mask_pos,
                   BOOL invers_mask)
{
    QXLDrawable *drawable;
    UINT32 width;
    UINT32 height;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    ASSERT(pdev, pdev && area && brush);

    if (!(drawable = Drawable(pdev, QXL_DRAW_FILL, area, clip, surface_id))) {
        return FALSE;
    }

    width = area->right - area->left;
    height = area->bottom - area->top;

    if (!QXLGetBrush(pdev, drawable, &drawable->u.fill.brush, brush, brush_pos,
                     &drawable->surfaces_dest[0], &drawable->surfaces_rects[0]) ||
        !QXLGetMask(pdev, drawable, &drawable->u.fill.mask, mask, mask_pos, invers_mask,
                     width, height, &drawable->surfaces_dest[1])) {
        ReleaseOutput(pdev, drawable->release_info.id);
        return FALSE;
    }

    drawable->u.fill.rop_descriptor = rop_info->method_data;

    drawable->effect = mask ? QXL_EFFECT_BLEND : rop_info->effect;

    if (mask_pos) {
        CopyRectPoint(&drawable->surfaces_rects[1], mask_pos, width, height);
    }

    PushDrawable(pdev, drawable);
    return TRUE;
}

static BOOL GetBitmap(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *bitmap_phys, SURFOBJ *surf,
                      QXLRect *area, XLATEOBJ *color_trans, BOOL use_cache, INT32 *surface_dest)
{
    DEBUG_PRINT((pdev, 9, "%s\n", __FUNCTION__));
    if (surf->iType != STYPE_BITMAP) {
        UINT32 surface_id;

        ASSERT(pdev, (PDev *)surf->dhpdev == pdev);
        surface_id =  GetSurfaceId(surf);
        if (surface_id == drawable->surface_id) {
            DEBUG_PRINT((pdev, 9, "%s copy from self\n", __FUNCTION__));
            *bitmap_phys = 0;
            drawable->self_bitmap = TRUE;
            drawable->self_bitmap_area = *area;
            area->right = area->right - area->left;
            area->left = 0;
            area->bottom = area->bottom - area->top;
            area->top = 0;
            return TRUE;
        }
    }
    return QXLGetBitmap(pdev, drawable, &drawable->u.opaque.src_bitmap, surf,
                        area, color_trans, NULL, use_cache, surface_dest);
}

static _inline UINT8 GdiScaleModeToQxl(ULONG scale_mode)
{
    return (scale_mode == HALFTONE) ? SPICE_IMAGE_SCALE_MODE_INTERPOLATE :
                                      SPICE_IMAGE_SCALE_MODE_NEAREST;
}

static BOOL DoOpaque(PDev *pdev, UINT32 surface_id, RECTL *area, CLIPOBJ *clip, SURFOBJ *src,
                     RECTL *src_rect, XLATEOBJ *color_trans, BRUSHOBJ *brush, POINTL *brush_pos,
                     UINT16 rop_descriptor, SURFOBJ *mask, POINTL *mask_pos, BOOL invers_mask,
                     ULONG scale_mode)
{
    QXLDrawable *drawable;
    UINT32 width;
    UINT32 height;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    ASSERT(pdev, pdev && area && brush && src_rect && src);

    if (!(drawable = Drawable(pdev, QXL_DRAW_OPAQUE, area, clip, surface_id))) {
        return FALSE;
    }

    drawable->u.opaque.scale_mode = GdiScaleModeToQxl(scale_mode);
    CopyRect(&drawable->u.opaque.src_area, src_rect);

    width = area->right - area->left;
    height = area->bottom - area->top;

    if (!QXLGetBrush(pdev, drawable, &drawable->u.opaque.brush, brush, brush_pos,
                     &drawable->surfaces_dest[0], &drawable->surfaces_rects[0]) ||
        !QXLGetMask(pdev, drawable, &drawable->u.opaque.mask, mask, mask_pos, invers_mask,
                    width, height, &drawable->surfaces_dest[1]) ||
        !GetBitmap(pdev, drawable, &drawable->u.opaque.src_bitmap, src,
                   &drawable->u.opaque.src_area, color_trans, TRUE,
                   &drawable->surfaces_dest[2])) {
        ReleaseOutput(pdev, drawable->release_info.id);
        return FALSE;
    }

    if (mask_pos) {
        CopyRectPoint(&drawable->surfaces_rects[1], mask_pos, width, height);
    }
    CopyRect(&drawable->surfaces_rects[2], src_rect);

    drawable->u.opaque.rop_descriptor = rop_descriptor;
    drawable->effect = mask ? QXL_EFFECT_BLEND : QXL_EFFECT_OPAQUE;
    PushDrawable(pdev, drawable);
    return TRUE;
}

static BOOL StreamTest(PDev *pdev, UINT32 surface_id, SURFOBJ *src_surf, 
		       XLATEOBJ *color_trans, RECTL *src_rect, RECTL *dest)
{
    Ring *ring = &pdev->update_trace;
    UpdateTrace *trace = (UpdateTrace *)ring->next;
    LONG src_pixmap_pixels = src_surf->sizlBitmap.cx * src_surf->sizlBitmap.cy;

    if (src_pixmap_pixels <= 128 * 128 || 
	/* Only handle streams on primary surface */
	surface_id != 0) {
        return TRUE;
    }

    for (;;) {
        if (SameRect(dest, &trace->area) || (trace->hsurf == src_surf->hsurf &&
                                              src_pixmap_pixels / RectSize(src_rect) > 100)) {
            UINT32 now = *pdev->mm_clock;
            BOOL ret;

            if (now != trace->last_time && now - trace->last_time < 1000 / 5) {
                trace->last_time = now - 1; // asumong mm clock is active so delta t == 0 is
                                            // imposibole. frocing delata t to be at least 1.
                if (trace->count < 20) {
                    trace->count++;
                    ret = TRUE;
                } else {
                    ret = FALSE;
                }
            } else {
                trace->last_time = now;
                trace->count = 0;
                ret = TRUE;
            }
            RingRemove(pdev, (RingItem *)trace);
            RingAdd(pdev, ring, (RingItem *)trace);
            return ret;
        }
        if (trace->link.next == ring) {
            break;
        }
        trace = (UpdateTrace *)trace->link.next;
    }
    RingRemove(pdev, (RingItem *)trace);
    trace->area = *dest;
    trace->last_time = *pdev->mm_clock;

    if (IsUniqueSurf(src_surf, color_trans)) {
        trace->hsurf = src_surf->hsurf;
    } else {
        trace->hsurf = NULL;
    }
    trace->count = 0;
    RingAdd(pdev, ring, (RingItem *)trace);

    return TRUE;
}

static BOOL TestSplitClips(PDev *pdev, SURFOBJ *src, RECTL *src_rect, CLIPOBJ *clip, SURFOBJ *mask)
{
    UINT32 width;
    UINT32 height;
    UINT32 src_space;
    UINT32 clip_space = 0;
    int more;

    if (!clip || mask) {
        return FALSE;
    }

    if (src->iType != STYPE_BITMAP) {
        return FALSE;
    }

    width = src_rect->right - src_rect->left;
    height = src_rect->bottom - src_rect->top;
    src_space = width * height;

    if (clip->iDComplexity == DC_RECT) {
        width = clip->rclBounds.right - clip->rclBounds.left;
        height = clip->rclBounds.bottom - clip->rclBounds.top;
        clip_space = width * height;

        if ((src_space / clip_space) > 1) {
            return TRUE;
        }
        return FALSE;
    }

    if (clip->iMode == TC_RECTANGLES) {
        CLIPOBJ_cEnumStart(clip, TRUE, CT_RECTANGLES, CD_RIGHTDOWN, 0);
        do {
            RECTL *now;
            RECTL *end;

            struct {
                ULONG  count;
                RECTL  rects[20];
            } buf;

            more = CLIPOBJ_bEnum(clip, sizeof(buf), (ULONG *)&buf);
            for(now = buf.rects, end = now + buf.count; now < end; now++) {
                width = now->right - now->left;
                height = now->bottom - now->top;
                clip_space += width * height;
            }
        } while (more);

        if ((src_space / clip_space) > 1) {
            return TRUE;
        }
    }
    return FALSE;
}

static _inline BOOL DoPartialCopy(PDev *pdev, UINT32 surface_id, SURFOBJ *src, RECTL *src_rect,
                                  RECTL *area_rect, RECTL *clip_rect, XLATEOBJ *color_trans,
                                  ULONG scale_mode, UINT16 rop_descriptor)
{
    QXLDrawable *drawable;
    RECTL clip_area;
    UINT32 width;
    UINT32 height;

    SectRect(area_rect, clip_rect, &clip_area);
    if (IsEmptyRect(&clip_area)) {
        return TRUE;
    }

    width = clip_area.right - clip_area.left;
    height = clip_area.bottom - clip_area.top;

    if (!(drawable = Drawable(pdev, QXL_DRAW_COPY, &clip_area, NULL, surface_id))) {
        return FALSE;
    }

    drawable->effect = QXL_EFFECT_OPAQUE;
    drawable->u.copy.scale_mode = GdiScaleModeToQxl(scale_mode);
    drawable->u.copy.mask.bitmap = 0;
    drawable->u.copy.rop_descriptor = rop_descriptor;

    drawable->u.copy.src_area.top = src_rect->top + (clip_area.top - area_rect->top);
    drawable->u.copy.src_area.bottom = drawable->u.copy.src_area.top + clip_area.bottom -
                                       clip_area.top;
    drawable->u.copy.src_area.left = src_rect->left + (clip_area.left - area_rect->left);
    drawable->u.copy.src_area.right = drawable->u.copy.src_area.left + clip_area.right -
                                      clip_area.left;

    if(!GetBitmap(pdev, drawable, &drawable->u.copy.src_bitmap, src, &drawable->u.copy.src_area,
                  color_trans, FALSE, &drawable->surfaces_dest[0])) {
        ReleaseOutput(pdev, drawable->release_info.id);
        return FALSE;
    }
    CopyRect(&drawable->surfaces_rects[0], src_rect);
    PushDrawable(pdev, drawable);
    return TRUE;
}

static BOOL DoCopy(PDev *pdev, UINT32 surface_id, RECTL *area, CLIPOBJ *clip, SURFOBJ *src,
                   RECTL *src_rect, XLATEOBJ *color_trans, UINT16 rop_descriptor, SURFOBJ *mask,
                   POINTL *mask_pos, BOOL invers_mask, ULONG scale_mode)
{
    QXLDrawable *drawable;
    BOOL use_cache;
    UINT32 width;
    UINT32 height;

    ASSERT(pdev, pdev && area && src_rect && src);
    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));

    width = area->right - area->left;
    height = area->bottom - area->top;

    if (mask) {
        use_cache = TRUE;
    } else {
        use_cache = StreamTest(pdev, surface_id, src, color_trans, src_rect, area);
    }

    if (use_cache && TestSplitClips(pdev, src, src_rect, clip, mask) &&
        !QXLCheckIfCacheImage(pdev, src, color_trans)) {
        if (clip->iDComplexity == DC_RECT) {
            if (!DoPartialCopy(pdev, surface_id, src, src_rect, area, &clip->rclBounds, color_trans,
                               scale_mode, rop_descriptor)) {
                return FALSE;
            }
        } else {
            int more;
            ASSERT(pdev, clip->iMode == TC_RECTANGLES);
            CLIPOBJ_cEnumStart(clip, TRUE, CT_RECTANGLES, CD_RIGHTDOWN, 0);
            do {
                RECTL *now;
                RECTL *end;

                struct {
                    ULONG  count;
                    RECTL  rects[20];
                } buf;
                more = CLIPOBJ_bEnum(clip, sizeof(buf), (ULONG *)&buf);
                for(now = buf.rects, end = now + buf.count; now < end; now++) {
                    if (!DoPartialCopy(pdev, surface_id, src, src_rect, area, now, color_trans,
                                       scale_mode, rop_descriptor)) {
                        return FALSE;
                    }
                }
            } while (more);
        }
        return TRUE;
    }

    if (!(drawable = Drawable(pdev, QXL_DRAW_COPY, area, clip, surface_id))) {
        return FALSE;
    }

    if (mask) {
        drawable->effect = QXL_EFFECT_BLEND;
    } else {
        drawable->effect = QXL_EFFECT_OPAQUE;
    }

    drawable->u.copy.scale_mode = GdiScaleModeToQxl(scale_mode);
    CopyRect(&drawable->u.copy.src_area, src_rect);
    if (!QXLGetMask(pdev, drawable, &drawable->u.copy.mask, mask, mask_pos, invers_mask,
                    width, height, &drawable->surfaces_dest[0]) ||
        !GetBitmap(pdev, drawable, &drawable->u.copy.src_bitmap, src, &drawable->u.copy.src_area,
                   color_trans, use_cache, &drawable->surfaces_dest[1])) {
        ReleaseOutput(pdev, drawable->release_info.id);
        return FALSE;
    }

    if (mask_pos) {
        CopyRectPoint(&drawable->surfaces_rects[0], mask_pos, width, height);
    }
    CopyRect(&drawable->surfaces_rects[1], src_rect);

    drawable->u.copy.rop_descriptor = rop_descriptor;
    PushDrawable(pdev, drawable);
    DEBUG_PRINT((pdev, 7, "%s: done\n", __FUNCTION__));
    return TRUE;
}

static BOOL DoCopyBits(PDev *pdev, UINT32 surface_id, CLIPOBJ *clip, RECTL *area, POINTL *src_pos)
{
    QXLDrawable *drawable;
    UINT32 width;
    UINT32 height; 

    ASSERT(pdev, pdev && area && src_pos);
    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));

    if (area->left == src_pos->x && area->top == src_pos->y) {
        DEBUG_PRINT((pdev, 6, "%s: NOP\n", __FUNCTION__));
        return TRUE;
    }

    width = area->right - area->left;
    height = area->bottom - area->top;

    if (!(drawable = Drawable(pdev, QXL_COPY_BITS, area, clip, surface_id))) {
        return FALSE;
    }

    drawable->surfaces_dest[0] = surface_id;
    CopyRectPoint(&drawable->surfaces_rects[0], src_pos, width, height);

    CopyPoint(&drawable->u.copy_bits.src_pos, src_pos);
    drawable->effect = QXL_EFFECT_OPAQUE;
    PushDrawable(pdev, drawable);
    return TRUE;
}

static BOOL DoBlend(PDev *pdev, UINT32 surface_id, RECTL *area, CLIPOBJ *clip, SURFOBJ *src,
                    RECTL *src_rect, XLATEOBJ *color_trans, ROP3Info *rop_info, SURFOBJ *mask,
                    POINTL *mask_pos, BOOL invers_mask, ULONG scale_mode)
{
    QXLDrawable *drawable;
    UINT32 width;
    UINT32 height; 

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    ASSERT(pdev, pdev && area && src_rect && src);

    if (!(drawable = Drawable(pdev, QXL_DRAW_BLEND, area, clip, surface_id))) {
        return FALSE;
    }

    width = area->right - area->left;
    height = area->bottom - area->top;

    drawable->u.blend.scale_mode = GdiScaleModeToQxl(scale_mode);
    CopyRect(&drawable->u.blend.src_area, src_rect);
    if (!QXLGetMask(pdev, drawable, &drawable->u.blend.mask, mask, mask_pos, invers_mask,
                    width, height, &drawable->surfaces_dest[0]) ||
        !GetBitmap(pdev, drawable, &drawable->u.blend.src_bitmap, src, &drawable->u.blend.src_area,
                   color_trans, TRUE, &drawable->surfaces_dest[1])) {
        ReleaseOutput(pdev, drawable->release_info.id);
        return FALSE;
    }

    if (mask_pos) {
        CopyRectPoint(&drawable->surfaces_rects[0], mask_pos, width, height);
    }
    CopyRect(&drawable->surfaces_rects[1], src_rect);

    drawable->u.blend.rop_descriptor = rop_info->method_data;
    drawable->effect = mask ? QXL_EFFECT_BLEND : rop_info->effect;
    PushDrawable(pdev, drawable);
    return TRUE;
}

static BOOL DoBlackness(PDev *pdev, UINT32 surface_id, RECTL *area, CLIPOBJ *clip, SURFOBJ *mask,
                        POINTL *mask_pos, BOOL invers_mask)
{
    QXLDrawable *drawable;
    UINT32 width;
    UINT32 height;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    ASSERT(pdev, pdev && area);

    if (!(drawable = Drawable(pdev, QXL_DRAW_BLACKNESS, area, clip, surface_id))) {
        return FALSE;
    }

    width = area->right - area->left;
    height = area->bottom - area->top;

    if (!QXLGetMask(pdev, drawable, &drawable->u.blackness.mask, mask, mask_pos, invers_mask,
                    width, height, &drawable->surfaces_dest[0])) {
        ReleaseOutput(pdev, drawable->release_info.id);
        return FALSE;
    }

 if (mask_pos) {
        CopyRectPoint(&drawable->surfaces_rects[0], mask_pos, width, height);
 }

    drawable->effect = mask ? QXL_EFFECT_BLEND : QXL_EFFECT_OPAQUE;
    PushDrawable(pdev, drawable);
    return TRUE;
}

static BOOL DoWhiteness(PDev *pdev, UINT32 surface_id, RECTL *area, CLIPOBJ *clip, SURFOBJ *mask, 
                        POINTL *mask_pos, BOOL invers_mask)
{
    QXLDrawable *drawable;
    UINT32 width;
    UINT32 height;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    ASSERT(pdev, pdev && area);

    if (!(drawable = Drawable(pdev, QXL_DRAW_WHITENESS, area, clip, surface_id))) {
        return FALSE;
    }

    width = area->right - area->left;
    height = area->bottom - area->top;

    if (!QXLGetMask(pdev, drawable, &drawable->u.whiteness.mask, mask, mask_pos, invers_mask,
                    width, height, &drawable->surfaces_dest[0])) {
        ReleaseOutput(pdev, drawable->release_info.id);
        return FALSE;
    }

    if (mask_pos) {
        CopyRectPoint(&drawable->surfaces_rects[0], mask_pos, width, height);
    }

    drawable->effect = mask ? QXL_EFFECT_BLEND : QXL_EFFECT_OPAQUE;
    PushDrawable(pdev, drawable);
    return TRUE;
}

static BOOL DoInvers(PDev *pdev, UINT32 surface_id, RECTL *area, CLIPOBJ *clip, SURFOBJ *mask, 
                     POINTL *mask_pos, BOOL invers_mask)
{
    QXLDrawable *drawable;
    UINT32 width;
    UINT32 height;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    ASSERT(pdev, pdev && area);

    if (!(drawable = Drawable(pdev, QXL_DRAW_INVERS, area, clip, surface_id))) {
        return FALSE;
    }

    width = area->right - area->left;
    height = area->bottom - area->top;

    if (!QXLGetMask(pdev, drawable, &drawable->u.invers.mask, mask, mask_pos, invers_mask,
                    width, height, &drawable->surfaces_dest[0])) {
        ReleaseOutput(pdev, drawable->release_info.id);
        return FALSE;
    }

    if (mask_pos) {
        CopyRectPoint(&drawable->surfaces_rects[0], mask_pos, width, height);
    }

    drawable->effect = mask ? QXL_EFFECT_BLEND : QXL_EFFECT_REVERT_ON_DUP;
    PushDrawable(pdev, drawable);
    return TRUE;
}

static BOOL DoROP3(PDev *pdev, UINT32 surface_id, RECTL *area, CLIPOBJ *clip, SURFOBJ *src,
                   RECTL *src_rect, XLATEOBJ *color_trans, BRUSHOBJ *brush, POINTL *brush_pos,
                   UINT8 rop3, SURFOBJ *mask, POINTL *mask_pos, BOOL invers_mask, ULONG scale_mode)
{
    QXLDrawable *drawable;
    UINT32 width;
    UINT32 height;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    ASSERT(pdev, pdev && area && brush && src_rect && src);

    if (!(drawable = Drawable(pdev, QXL_DRAW_ROP3, area, clip, surface_id))) {
        return FALSE;
    }

    width = area->right - area->left;
    height = area->bottom - area->top;

    drawable->u.rop3.scale_mode = GdiScaleModeToQxl(scale_mode);
    CopyRect(&drawable->u.rop3.src_area, src_rect);
    if (!QXLGetBrush(pdev, drawable, &drawable->u.rop3.brush, brush, brush_pos,
                     &drawable->surfaces_dest[0], &drawable->surfaces_rects[0]) ||
        !QXLGetMask(pdev, drawable, &drawable->u.rop3.mask, mask, mask_pos, invers_mask,
                    width, height, &drawable->surfaces_dest[1]) ||
        !GetBitmap(pdev, drawable, &drawable->u.rop3.src_bitmap, src, &drawable->u.rop3.src_area,
                   color_trans, TRUE, &drawable->surfaces_dest[2])) {
        ReleaseOutput(pdev, drawable->release_info.id);
        return FALSE;
    }

    if (mask_pos) {
        CopyRectPoint(&drawable->surfaces_rects[1], mask_pos, width, height);
    }
    CopyRect(&drawable->surfaces_rects[2], src_rect);

    drawable->u.rop3.rop3 = rop3;
    drawable->effect = mask ? QXL_EFFECT_BLEND : QXL_EFFECT_BLEND; //for now
    PushDrawable(pdev, drawable);
    return TRUE;
}

BOOL BitBltFromDev(PDev *pdev, SURFOBJ *src, SURFOBJ *dest, SURFOBJ *mask, CLIPOBJ *clip,
                   XLATEOBJ *color_trans, RECTL *dest_rect, POINTL src_pos,
                   POINTL *mask_pos, BRUSHOBJ *brush, POINTL *brush_pos, ROP4 rop4)
{
    RECTL area;
    SURFOBJ* surf_obj;
    BOOL ret;
    UINT32 surface_id;
    SurfaceInfo *surface;

    surface = (SurfaceInfo *)src->dhsurf;
    surface_id = GetSurfaceId(src);

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));

    area.top = MAX(0, src_pos.y);
    area.bottom = MIN(src_pos.y + dest_rect->bottom - dest_rect->top,
                      surface->draw_area.surf_obj->sizlBitmap.cy);
    area.left = MAX(0, src_pos.x);
    area.right = MIN(src_pos.x + dest_rect->right - dest_rect->left,
                     surface->draw_area.surf_obj->sizlBitmap.cx);

    UpdateArea(pdev, &area, surface_id);

    surf_obj = surface->draw_area.surf_obj;

    if (rop4 == 0xcccc) {
        ret = EngCopyBits(dest, surf_obj, clip, color_trans, dest_rect, &src_pos);
    } else {
        ret = EngBitBlt(dest, surf_obj, mask, clip, color_trans, dest_rect, &src_pos,
                        mask_pos, brush, brush_pos, rop4);
    }

    return ret;
}

BOOL _inline __DrvBitBlt(PDev *pdev, UINT32 surface_id, RECTL *dest_rect, CLIPOBJ *clip,
                        SURFOBJ  *src, RECTL *src_rect, XLATEOBJ *color_trans, BRUSHOBJ *brush, 
                        POINTL *brush_pos, ULONG rop3, SURFOBJ *mask, POINTL *mask_pos,
                        BOOL invers_mask, ULONG scale_mode)
{
    ROP3Info *rop_info = &rops3[rop3];

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));

    switch (rop_info->method_type) {
    case ROP3_TYPE_FILL:
        return DoFill(pdev, surface_id, dest_rect, clip, brush, brush_pos, rop_info, mask, mask_pos,
                      invers_mask);
    case ROP3_TYPE_OPAQUE:
        return DoOpaque(pdev, surface_id, dest_rect, clip, src, src_rect, color_trans, brush,
                        brush_pos, rop_info->method_data, mask, mask_pos, invers_mask, scale_mode);
    case ROP3_TYPE_COPY:
        return DoCopy(pdev, surface_id, dest_rect, clip, src, src_rect, color_trans,
                      rop_info->method_data, mask, mask_pos, invers_mask, scale_mode);
    case ROP3_TYPE_BLEND:
        return DoBlend(pdev, surface_id, dest_rect, clip, src, src_rect, color_trans, rop_info,
                       mask, mask_pos, invers_mask, scale_mode);
    case ROP3_TYPE_BLACKNESS:
        return DoBlackness(pdev, surface_id, dest_rect, clip, mask, mask_pos, invers_mask);
    case ROP3_TYPE_WHITENESS:
        return DoWhiteness(pdev, surface_id, dest_rect, clip, mask, mask_pos, invers_mask);
    case ROP3_TYPE_INVERS:
        return DoInvers(pdev, surface_id, dest_rect, clip, mask, mask_pos, invers_mask);
    case ROP3_TYPE_ROP3:
        return DoROP3(pdev, surface_id, dest_rect, clip, src, src_rect, color_trans, brush,
                      brush_pos, (UINT8)rop_info->method_data, mask, mask_pos, invers_mask,
                      scale_mode);
    case ROP3_TYPE_NOP:
        return TRUE;
    default:
        DEBUG_PRINT((pdev, 0, "%s: Error\n", __FUNCTION__));
        //EngSetError
        return FALSE;
    }
}

#ifdef SUPPORT_BRUSH_AS_MASK
SURFOBJ *BrushToMask(PDev *pdev, BRUSHOBJ *brush)
{
    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));

    if (!brush || brush->iSolidColor != ~0) {
        DEBUG_PRINT((pdev, 8, "%s: no mask, brush 0x%x color 0x%x\n",
                     __FUNCTION__, brush, brush ? brush->iSolidColor : 0));
        return NULL;
    }

    if (!brush->pvRbrush && !BRUSHOBJ_pvGetRbrush(brush)) {
        DEBUG_PRINT((pdev, 0, "%s: realize failed\n", __FUNCTION__));
        return NULL;
    }
    DEBUG_PRINT((pdev, 7, "%s: done 0x%x\n", __FUNCTION__, brush->pvRbrush));
    return NULL;
}
#endif


static _inline BOOL TestSrcBits(PDev *pdev, SURFOBJ *src, XLATEOBJ *color_trans)
{
    if (src) {
        switch (src->iBitmapFormat) {
        case BMF_32BPP:
        case BMF_24BPP:
        case BMF_16BPP: {
            ULONG bit_fields[3];
            ULONG ents;

            if (!color_trans || (color_trans->flXlate & XO_TRIVIAL)) {
                return TRUE;
            }

            ents = XLATEOBJ_cGetPalette(color_trans, XO_SRCBITFIELDS, 3, bit_fields);
            ASSERT(pdev, ents == 3);
            switch (src->iBitmapFormat) {
            case BMF_32BPP:
            case BMF_24BPP:
                if (bit_fields[0] != 0x00ff0000 || bit_fields[1] != 0x0000ff00 ||
                                                                    bit_fields[2] != 0x000000ff) {
                    DEBUG_PRINT((pdev, 11, "%s: BMF_32BPP/24BPP r 0x%x g 0x%x b 0x%x\n",
                                 __FUNCTION__,
                                 bit_fields[0],
                                 bit_fields[1],
                                 bit_fields[2]));
                    return FALSE;
                }
                break;
            case BMF_16BPP:
                if (bit_fields[0] != 0x7c00 || bit_fields[1] != 0x03e0 ||
                                                                        bit_fields[2] != 0x001f) {
                    DEBUG_PRINT((pdev, 11, "%s: BMF_16BPP r 0x%x g 0x%x b 0x%x\n",
                                 __FUNCTION__,
                                 bit_fields[0],
                                 bit_fields[1],
                                 bit_fields[2]));
                    return FALSE;
                }
                break;
            }
            return TRUE;
        }
        case BMF_8BPP:
        case BMF_4BPP:
        case BMF_1BPP:
            return color_trans && (color_trans->flXlate & XO_TABLE);
        default:
            return FALSE;
        }
    }
    return TRUE;
}

static QXLRESULT BitBltCommon(PDev *pdev, SURFOBJ *dest, SURFOBJ *src, SURFOBJ *mask, CLIPOBJ *clip,
                              XLATEOBJ *color_trans, RECTL *dest_rect, RECTL *src_rect,
                              POINTL *mask_pos, BRUSHOBJ *brush, POINTL *brush_pos, ROP4 rop4,
                              ULONG scale_mode, COLORADJUSTMENT *color_adjust)
{
    ULONG rop3;
    ULONG second_rop3;
#ifdef SUPPORT_BRUSH_AS_MASK
    SURFOBJ *brush_mask = NULL;
#endif
    QXLRESULT res;
    UINT32 surface_id;

    ASSERT(pdev, dest->iType != STYPE_BITMAP);

    surface_id = GetSurfaceId(dest);

    if (!PrepareBrush(brush)) {
        return QXL_FAILED;
    }

    if ((rop3 = rop4 & 0xff) == (second_rop3 = ((rop4 >> 8) & 0xff))) {
        return __DrvBitBlt(pdev, surface_id, dest_rect, clip, src, src_rect, color_trans, brush,
                           brush_pos, rop3, NULL, NULL, FALSE, scale_mode) ? QXL_SUCCESS :
                           QXL_FAILED;
    }

    if (!mask) {
        DEBUG_PRINT((pdev, 5, "%s: no mask. rop4 is 0x%x\n", __FUNCTION__, rop4));
        return QXL_UNSUPPORTED;
#ifdef SUPPORT_BRUSH_AS_MASK
        brush_mask = BrushToMask(pdev, brush);
        if (!brush_mask) {
            DEBUG_PRINT((pdev, 5, "%s: no mask. rop4 is 0x%x\n", __FUNCTION__, rop4));
            return QXL_UNSUPPORTED;
        }
        mask = brush_mask;
        ASSERT(pdev, mask_pos);
#endif
    }
    DEBUG_PRINT((pdev, 5, "%s: mask, rop4 is 0x%x\n", __FUNCTION__, rop4));
    ASSERT(pdev, mask_pos);
    res = (__DrvBitBlt(pdev, surface_id, dest_rect, clip, src, src_rect, color_trans, brush,
                       brush_pos, rop3, mask, mask_pos, FALSE, scale_mode) &&
          __DrvBitBlt(pdev, surface_id, dest_rect, clip, src, src_rect, color_trans, brush,
                      brush_pos, second_rop3, mask, mask_pos, TRUE, scale_mode)) ? QXL_SUCCESS :
                      QXL_FAILED;
#ifdef SUPPORT_BRUSH_AS_MASK
    if (brush_mask) {
        //free brush_mask;
    }
#endif

    return res;
}

static _inline void FixDestParams(PDev *pdev, SURFOBJ *dest, CLIPOBJ **in_clip,
                                  RECTL *dest_rect, RECTL *area, POINTL **in_mask_pos,
                                  POINTL *local_mask_pos)
{
    CLIPOBJ *clip;

    area->top = MAX(dest_rect->top, 0);
    area->left = MAX(dest_rect->left, 0);
    area->bottom = MIN(dest->sizlBitmap.cy, dest_rect->bottom);
    area->right = MIN(dest->sizlBitmap.cx, dest_rect->right);

    clip = *in_clip;
    if (clip) {
        if (clip->iDComplexity == DC_TRIVIAL) {
            clip = NULL;
        } else {
            SectRect(&clip->rclBounds, area, area);
            if (clip->iDComplexity == DC_RECT) {
                clip = NULL;
            }
        }
        *in_clip = clip;
    }

    if (in_mask_pos && *in_mask_pos) {
        POINTL *mask_pos;
        ASSERT(pdev, local_mask_pos);
        mask_pos = *in_mask_pos;
        local_mask_pos->x = mask_pos->x + (area->left - dest_rect->left);
        local_mask_pos->y = mask_pos->y + (area->top - dest_rect->top);
        *in_mask_pos = local_mask_pos;
    }
}

static QXLRESULT _BitBlt(PDev *pdev, SURFOBJ *dest, SURFOBJ *src, SURFOBJ *mask, CLIPOBJ *clip,
                         XLATEOBJ *color_trans, RECTL *dest_rect, POINTL *src_pos,
                         POINTL *mask_pos, BRUSHOBJ *brush, POINTL *brush_pos, ROP4 rop4)
{
    RECTL area;
    POINTL local_mask_pos;
    RECTL src_rect;
    RECTL *src_rect_ptr;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));

    if (!TestSrcBits(pdev, src, color_trans)) {
        DEBUG_PRINT((pdev, 1, "%s: test src failed\n", __FUNCTION__));

        return EngBitBlt(dest, src, mask, clip, color_trans, dest_rect, src_pos, mask_pos, brush,
                         brush_pos, rop4) ? QXL_SUCCESS : QXL_FAILED;
    }

#if 0
    if (rop4 == 0xccaa) {
        DEBUG_PRINT((pdev, 7, "%s: rop4 is 0xccaa, call EngBitBlt\n", __FUNCTION__));
        return QXL_UNSUPPORTED;
    }
#endif

    ASSERT(pdev, dest_rect && dest_rect->left < dest_rect->right &&
           dest_rect->top < dest_rect->bottom);

    FixDestParams(pdev, dest, &clip, dest_rect, &area, &mask_pos, &local_mask_pos);
    if (IsEmptyRect(&area)) {
        DEBUG_PRINT((pdev, 0, "%s: empty rect\n", __FUNCTION__));
        return QXL_SUCCESS;
    }

    if (src && src_pos) {
        POINTL local_pos;

        local_pos.x = src_pos->x + (area.left - dest_rect->left);
        local_pos.y = src_pos->y + (area.top - dest_rect->top);

        if (dest->iType == STYPE_BITMAP) {
            return BitBltFromDev(pdev, src, dest, mask, clip, color_trans, &area, local_pos,
                                 mask_pos, brush, brush_pos, rop4) ? QXL_SUCCESS : QXL_FAILED;
        }

        if (src->iType != STYPE_BITMAP
            && GetSurfaceId(src) == GetSurfaceId(dest) && rop4 == 0xcccc) { //SRCCOPY no mask
            return DoCopyBits(pdev, GetSurfaceId(src), clip, &area, &local_pos) ?
                              QXL_SUCCESS : QXL_FAILED;
        }

        src_rect.left = local_pos.x;
        src_rect.right = src_rect.left + (area.right - area.left);
        src_rect.top = local_pos.y;
        src_rect.bottom = src_rect.top + (area.bottom - area.top);
        src_rect_ptr = &src_rect;
    } else {
        src_rect_ptr = NULL;
    }

    return BitBltCommon(pdev, dest, src, mask, clip, color_trans, &area, src_rect_ptr,
                        mask_pos, brush, brush_pos, rop4, COLORONCOLOR, NULL);
}

BOOL APIENTRY DrvBitBlt(SURFOBJ *dest, SURFOBJ *src, SURFOBJ *mask, CLIPOBJ *clip,
                      XLATEOBJ *color_trans, RECTL *dest_rect, POINTL *src_pos,
                      POINTL *mask_pos, BRUSHOBJ *brush, POINTL *brush_pos, ROP4 rop4)
{
    PDev *pdev;
    QXLRESULT res;

    if (dest->iType == STYPE_BITMAP) {
        pdev = (PDev *)src->dhpdev;
    } else {
        pdev = (PDev *)dest->dhpdev;
    }

    PUNT_IF_DISABLED(pdev);

    CountCall(pdev, CALL_COUNTER_BIT_BLT);

    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));
    if ((res = _BitBlt(pdev, dest, src, mask, clip, color_trans, dest_rect, src_pos, mask_pos,
                       brush, brush_pos, rop4))) {
        if (res == QXL_UNSUPPORTED) {
            DEBUG_PRINT((pdev, 4, "%s: call EngBitBlt\n", __FUNCTION__));
            return EngBitBlt(dest, src, mask, clip, color_trans, dest_rect, src_pos, mask_pos,
                             brush, brush_pos, rop4);
        }
        return FALSE;

    }

    DEBUG_PRINT((pdev, 4, "%s: done\n", __FUNCTION__));
    return TRUE;
}

BOOL APIENTRY DrvCopyBits(SURFOBJ *dest, SURFOBJ *src, CLIPOBJ *clip,
                          XLATEOBJ *color_trans, RECTL *dest_rect, POINTL *src_pos)
{
    PDev *pdev;

    if (dest->iType == STYPE_BITMAP) {
        pdev = (PDev *)src->dhpdev;
    } else {
        pdev = (PDev *)dest->dhpdev;
    }

    PUNT_IF_DISABLED(pdev);

    CountCall(pdev, CALL_COUNTER_BIT_BLT);

    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));

    return _BitBlt(pdev, dest, src, NULL, clip, color_trans, dest_rect, src_pos, NULL, NULL,
                   NULL, /*SRCCOPY*/ 0xcccc) == QXL_SUCCESS ? TRUE : FALSE;
}

static _inline BOOL TestStretchCondition(PDev *pdev, SURFOBJ *src, XLATEOBJ *color_trans,
                                         COLORADJUSTMENT *color_adjust,
                                         RECTL *dest_rect, RECTL *src_rect)
{
    int src_size;
    int dest_size;

    if (color_adjust && (color_adjust->caFlags & CA_NEGATIVE)) {
        return FALSE;
    }

    if (IsCacheableSurf(src, color_trans)) {
        return TRUE;
    }

    src_size = (src_rect->right - src_rect->left) * (src_rect->bottom - src_rect->top);
    dest_size = (dest_rect->right - dest_rect->left) * (dest_rect->bottom - dest_rect->top);

    return dest_size - src_size >= -(src_size >> 2);

}

static _inline unsigned int Scale(unsigned int val, unsigned int base_unit, unsigned int dest_unit)
{
    unsigned int div;
    unsigned int mod;

    div = dest_unit * val / base_unit;
    mod = dest_unit * val % base_unit;
    return (mod >= (base_unit >> 1)) ? div + 1: div;
}

static QXLRESULT _StretchBlt(PDev *pdev, SURFOBJ *dest, SURFOBJ *src, SURFOBJ *mask, CLIPOBJ *clip,
                             XLATEOBJ *color_trans, COLORADJUSTMENT *color_adjust,
                             POINTL *brush_pos, RECTL *dest_rect, RECTL *src_rect,
                             POINTL *mask_pos, ULONG mode, BRUSHOBJ *brush, DWORD rop4)
{
    RECTL area;
    POINTL local_mask_pos;
    RECTL local_dest_rect;
    RECTL local_src_rect;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));

    ASSERT(pdev, src_rect && src_rect->left < src_rect->right &&
           src_rect->top < src_rect->bottom);
    ASSERT(pdev, dest_rect);


    if (dest_rect->left > dest_rect->right) {
        local_dest_rect.left = dest_rect->right;
        local_dest_rect.right = dest_rect->left;
    } else {
        local_dest_rect.left = dest_rect->left;
        local_dest_rect.right = dest_rect->right;
    }

    if (dest_rect->top > dest_rect->bottom) {
        local_dest_rect.top = dest_rect->bottom;
        local_dest_rect.bottom = dest_rect->top;
    } else {
        local_dest_rect.top = dest_rect->top;
        local_dest_rect.bottom = dest_rect->bottom;
    }

    if (!TestSrcBits(pdev, src, color_trans)) {
        DEBUG_PRINT((pdev, 1, "%s: test src failed\n", __FUNCTION__));
        return QXL_UNSUPPORTED;
    }

    if (!TestStretchCondition(pdev, src, color_trans, color_adjust, &local_dest_rect, src_rect)) {
        DEBUG_PRINT((pdev, 1, "%s: stretch test failed\n", __FUNCTION__));
        return QXL_UNSUPPORTED;
    }

    FixDestParams(pdev, dest, &clip, &local_dest_rect, &area, &mask_pos, &local_mask_pos);
    if (IsEmptyRect(&area)) {
        DEBUG_PRINT((pdev, 0, "%s: empty dest\n", __FUNCTION__));
        return QXL_SUCCESS;
    }
    //todo: use FixStreatchSrcArea
    if (!SameRect(&local_dest_rect, &area)) { // possibly generate incosistent rendering on dest
                                              // edges
        unsigned int w_dest;
        unsigned int h_dest;

        if ((w_dest = local_dest_rect.right - local_dest_rect.left) != area.right - area.left) {
            unsigned int w_src = src_rect->right - src_rect->left;
            unsigned int delta;

            if ((delta = area.left - local_dest_rect.left)) {
                local_src_rect.left = src_rect->left + Scale(delta, w_dest, w_src);
            } else {
                local_src_rect.left = src_rect->left;
            }

            if ((delta = local_dest_rect.right - area.right)) {
                local_src_rect.right = src_rect->right - Scale(delta, w_dest, w_src);
            } else {
                local_src_rect.right = src_rect->right;
            }

            local_src_rect.left = MIN(local_src_rect.left, src->sizlBitmap.cx - 1);
            local_src_rect.right = MAX(local_src_rect.right, local_src_rect.left + 1);

        } else {
            local_src_rect.left = src_rect->left;
            local_src_rect.right = src_rect->right;
        }

        if ((h_dest = local_dest_rect.bottom - local_dest_rect.top) != area.bottom - area.top) {
            unsigned int h_src = src_rect->bottom - src_rect->top;
            unsigned int delta;

            if ((delta = area.top - local_dest_rect.top)) {
                local_src_rect.top = src_rect->top + Scale(delta, h_dest, h_src);
            } else {
                local_src_rect.top = src_rect->top;
            }

            if ((delta = local_dest_rect.bottom - area.bottom)) {
                local_src_rect.bottom = src_rect->bottom - Scale(delta, h_dest, h_src);
            } else {
                local_src_rect.bottom = src_rect->bottom;
            }

            local_src_rect.top = MIN(local_src_rect.top, src->sizlBitmap.cy - 1);
            local_src_rect.bottom = MAX(local_src_rect.bottom, local_src_rect.top + 1);

        } else {
            local_src_rect.top = src_rect->top;
            local_src_rect.bottom = src_rect->bottom;
        }

        src_rect = &local_src_rect;
    }

    return BitBltCommon(pdev, dest, src, mask, clip, color_trans, &area, src_rect, mask_pos,
                        brush, brush_pos, rop4, mode, color_adjust);
}

BOOL APIENTRY DrvStretchBltROP(SURFOBJ *dest, SURFOBJ *src, SURFOBJ *mask, CLIPOBJ *clip,
                               XLATEOBJ *color_trans, COLORADJUSTMENT *color_adjust,
                               POINTL *brush_pos, RECTL *dest_rect, RECTL *src_rect,
                               POINTL *mask_pos, ULONG mode, BRUSHOBJ *brush, DWORD rop4)
{
    PDev *pdev;
    QXLRESULT res;

    if (src && src->iType != STYPE_BITMAP) {
        pdev = (PDev *)src->dhpdev;
    } else {
        pdev = (PDev *)dest->dhpdev;
    }

    pdev = (PDev *)dest->dhpdev;
    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));
    CountCall(pdev, CALL_COUNTER_STRETCH_BLT_ROP);

    PUNT_IF_DISABLED(pdev);

    if ((res = _StretchBlt(pdev, dest, src, mask, clip, color_trans,
                           mode == HALFTONE ? color_adjust: NULL, brush_pos,
                           dest_rect, src_rect, mask_pos, mode, brush,rop4))) {
        if (res == QXL_UNSUPPORTED) {
            goto punt;
        }
        return FALSE;
    }
    return TRUE;

punt:
    return EngStretchBltROP(dest, src, mask, clip, color_trans, color_adjust, brush_pos,
                                dest_rect, src_rect, mask_pos, mode, brush, rop4);
}

BOOL APIENTRY DrvStretchBlt(SURFOBJ *dest, SURFOBJ *src, SURFOBJ *mask, CLIPOBJ *clip,
                            XLATEOBJ *color_trans, COLORADJUSTMENT *color_adjust,
                            POINTL *halftone_brush_pos, RECTL *dest_rect, RECTL *src_rect,
                            POINTL *mask_pos, ULONG mode)
{
    PDev *pdev;
    QXLRESULT res;

    ASSERT(NULL, src);
    if (src->iType != STYPE_BITMAP) {
        pdev = (PDev *)src->dhpdev;
    } else {
        pdev = (PDev *)dest->dhpdev;
    }
    pdev = (PDev *)dest->dhpdev;

    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));
    CountCall(pdev, CALL_COUNTER_STRETCH_BLT);
    PUNT_IF_DISABLED(pdev);

    if ((res = _StretchBlt(pdev, dest, src, mask, clip, color_trans,
                           mode == HALFTONE ? color_adjust: NULL, NULL, dest_rect,
                           src_rect, mask_pos, mode, NULL, (mask) ? 0xccaa:  0xcccc))) {
        if (res == QXL_UNSUPPORTED) {
            goto punt;
        }
        return FALSE;
    }
    return TRUE;

punt:
    return EngStretchBlt(dest, src, mask, clip, color_trans, color_adjust, halftone_brush_pos,
                         dest_rect, src_rect, mask_pos, mode);
}

static BOOL FixStreatchSrcArea(const RECTL *orig_dest, const RECTL *dest, const RECTL *orig_src,
                               const SIZEL *bitmap_size, RECTL *src)
{
    unsigned int w_dest;
    unsigned int h_dest;

    if (SameRect(orig_dest, dest)) {
        return FALSE;
    }

    // possibly generate incosistent rendering on dest edges

    if ((w_dest = orig_dest->right - orig_dest->left) != dest->right - dest->left) {
        unsigned int w_src = orig_src->right - orig_src->left;
        unsigned int delta;

        if ((delta = dest->left - orig_dest->left)) {
            src->left = orig_src->left + Scale(delta, w_dest, w_src);
        } else {
            src->left = orig_src->left;
        }

        if ((delta = orig_dest->right - dest->right)) {
            src->right = orig_src->right - Scale(delta, w_dest, w_src);
        } else {
            src->right = orig_src->right;
        }

        src->left = MIN(src->left, bitmap_size->cx - 1);
        src->right = MAX(src->right, src->left + 1);

    } else {
        src->left = orig_src->left;
        src->right = orig_src->right;
    }

    if ((h_dest = orig_dest->bottom - orig_dest->top) != dest->bottom - dest->top) {
        unsigned int h_src = orig_src->bottom - orig_src->top;
        unsigned int delta;

        if ((delta = dest->top - orig_dest->top)) {
            src->top = orig_src->top + Scale(delta, h_dest, h_src);
        } else {
            src->top = orig_src->top;
        }

        if ((delta = orig_dest->bottom - dest->bottom)) {
            src->bottom = orig_src->bottom - Scale(delta, h_dest, h_src);
        } else {
            src->bottom = orig_src->bottom;
        }

        src->top = MIN(src->top, bitmap_size->cy - 1);
        src->bottom = MAX(src->bottom, src->top + 1);

    } else {
        src->top = orig_src->top;
        src->bottom = orig_src->bottom;
    }

    return TRUE;
}

BOOL APIENTRY DrvAlphaBlend(SURFOBJ *dest, SURFOBJ *src, CLIPOBJ *clip, XLATEOBJ *color_trans,
                            RECTL *dest_rect, RECTL *src_rect, BLENDOBJ *bland)
{
    QXLDrawable *drawable;
    PDev *pdev;
    RECTL area;
    RECTL local_src;

    ASSERT(NULL, src && dest);
    if (src->iType != STYPE_BITMAP) {
        pdev = (PDev *)src->dhpdev;
    } else {
        pdev = (PDev *)dest->dhpdev;
    }

    pdev = (PDev *)dest->dhpdev;
    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));

    PUNT_IF_DISABLED(pdev);

    ASSERT(pdev, src_rect && src_rect->left < src_rect->right &&
           src_rect->top < src_rect->bottom);
    ASSERT(pdev, dest_rect && dest_rect->left < dest_rect->right &&
           dest_rect->top < dest_rect->bottom);

    CountCall(pdev, CALL_COUNTER_ALPHA_BLEND);

    if (bland->BlendFunction.BlendOp != AC_SRC_OVER) {
        DEBUG_PRINT((pdev, 0, "%s: unexpected BlendOp\n", __FUNCTION__));
        goto punt;
    }

    if (bland->BlendFunction.SourceConstantAlpha == 0) {
        return TRUE;
    }

    if (!TestSrcBits(pdev, src, color_trans)) {
        DEBUG_PRINT((pdev, 1, "%s: test src failed\n", __FUNCTION__));
        goto punt;
    }

    if (!TestStretchCondition(pdev, src, color_trans, NULL, dest_rect, src_rect)) {
        DEBUG_PRINT((pdev, 1, "%s: stretch test failed\n", __FUNCTION__));
        goto punt;
    }

    FixDestParams(pdev, dest, &clip, dest_rect, &area, NULL, NULL);
    if (IsEmptyRect(&area)) {
        DEBUG_PRINT((pdev, 0, "%s: empty dest\n", __FUNCTION__));
        return TRUE;
    }

    if (!(drawable = Drawable(pdev, QXL_DRAW_ALPHA_BLEND, &area, clip, GetSurfaceId(dest)))) {
        DEBUG_PRINT((pdev, 0, "%s: Drawable failed\n", __FUNCTION__));
        return FALSE;
    }

    if (FixStreatchSrcArea(dest_rect, &area, src_rect, &src->sizlBitmap, &local_src)) {
        src_rect = &local_src;
    }

    CopyRect(&drawable->surfaces_rects[0], src_rect);
    CopyRect(&drawable->u.alpha_blend.src_area, src_rect);
    if (bland->BlendFunction.AlphaFormat == AC_SRC_ALPHA) {
        ASSERT(pdev, src->iBitmapFormat == BMF_32BPP);
        if (!QXLGetAlphaBitmap(pdev, drawable, &drawable->u.alpha_blend.src_bitmap, src,
                               &drawable->u.alpha_blend.src_area,
                               &drawable->surfaces_dest[0], color_trans)) {
            DEBUG_PRINT((pdev, 0, "%s: QXLGetAlphaBitmap failed\n", __FUNCTION__));
            ReleaseOutput(pdev, drawable->release_info.id);
            return FALSE;
        }
    } else {
        if (!QXLGetBitmap(pdev, drawable, &drawable->u.alpha_blend.src_bitmap, src,
                        &drawable->u.alpha_blend.src_area, color_trans, NULL, TRUE,
                        &drawable->surfaces_dest[0])) {
            DEBUG_PRINT((pdev, 0, "%s: QXLGetBitmap failed\n", __FUNCTION__));
            ReleaseOutput(pdev, drawable->release_info.id);
            return FALSE;
        }
    }
    drawable->u.alpha_blend.alpha_flags = 0;
    if (src->iType != STYPE_BITMAP && 
	bland->BlendFunction.AlphaFormat == AC_SRC_ALPHA)
      drawable->u.alpha_blend.alpha_flags |= SPICE_ALPHA_FLAGS_SRC_SURFACE_HAS_ALPHA;
    
    drawable->u.alpha_blend.alpha = bland->BlendFunction.SourceConstantAlpha;
    drawable->effect = QXL_EFFECT_BLEND;

    PushDrawable(pdev, drawable);
    DEBUG_PRINT((pdev, 4, "%s: done\n", __FUNCTION__));

    return TRUE;

punt:
    return EngAlphaBlend(dest, src, clip, color_trans, dest_rect, src_rect, bland);
}

BOOL APIENTRY DrvTransparentBlt(SURFOBJ *dest, SURFOBJ *src, CLIPOBJ *clip, XLATEOBJ *color_trans,
                                RECTL *dest_rect, RECTL *src_rect, ULONG trans_color,
                                ULONG reserved)
{
    QXLDrawable *drawable;
    PDev *pdev;
    RECTL area;
    RECTL local_src;

    ASSERT(NULL, src && dest);
    if (src->iType != STYPE_BITMAP) {
        ASSERT(NULL, src->dhpdev);
        pdev = (PDev *)src->dhpdev;
    } else {
        ASSERT(NULL, dest->dhpdev);
        pdev = (PDev *)dest->dhpdev;
    }

    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));

    PUNT_IF_DISABLED(pdev);

    ASSERT(pdev, src_rect && src_rect->left < src_rect->right &&
           src_rect->top < src_rect->bottom);
    ASSERT(pdev, dest_rect && dest_rect->left < dest_rect->right &&
           dest_rect->top < dest_rect->bottom);

    CountCall(pdev, CALL_COUNTER_TRANSPARENT_BLT);

    if (!TestSrcBits(pdev, src, color_trans)) {
        DEBUG_PRINT((pdev, 1, "%s: test src failed\n", __FUNCTION__));
        goto punt;
    }

    if (!TestStretchCondition(pdev, src, color_trans, NULL, dest_rect, src_rect)) {
        DEBUG_PRINT((pdev, 1, "%s: stretch test failed\n", __FUNCTION__));
        goto punt;
    }

    FixDestParams(pdev, dest, &clip, dest_rect, &area, NULL, NULL);
    if (IsEmptyRect(&area)) {
        DEBUG_PRINT((pdev, 0, "%s: empty dest\n", __FUNCTION__));
        return TRUE;
    }

    if (!(drawable = Drawable(pdev, QXL_DRAW_TRANSPARENT, &area, clip, GetSurfaceId(dest)))) {
        DEBUG_PRINT((pdev, 0, "%s: Drawable failed\n", __FUNCTION__));
        return FALSE;
    }

    if (FixStreatchSrcArea(dest_rect, &area, src_rect, &src->sizlBitmap, &local_src)) {
        src_rect = &local_src;
    }

    CopyRect(&drawable->u.transparent.src_area, src_rect);
    CopyRect(&drawable->surfaces_rects[0], src_rect);
    if (!QXLGetBitmap(pdev, drawable, &drawable->u.transparent.src_bitmap, src,
                      &drawable->u.transparent.src_area, color_trans, NULL, TRUE,
                      &drawable->surfaces_dest[0])) {
        DEBUG_PRINT((pdev, 0, "%s: QXLGetBitmap failed\n", __FUNCTION__));
        ReleaseOutput(pdev, drawable->release_info.id);
        return FALSE;
    }

    drawable->u.transparent.src_color = trans_color;
    switch (src->iBitmapFormat) {
    case BMF_32BPP:
    case BMF_24BPP:
        drawable->u.transparent.true_color = trans_color;
        break;
    case BMF_16BPP:
        drawable->u.transparent.true_color = _16bppTo32bpp(trans_color);
        break;
    case BMF_8BPP:
    case BMF_4BPP:
    case BMF_1BPP:
        ASSERT(pdev, trans_color < color_trans->cEntries);
        if (pdev->bitmap_format == BMF_32BPP) {
            drawable->u.transparent.true_color = color_trans->pulXlate[trans_color];
        } else {
            ASSERT(pdev, pdev->bitmap_format == BMF_16BPP);
            drawable->u.transparent.true_color = _16bppTo32bpp(color_trans->pulXlate[trans_color]);
        }
        break;
            return color_trans && (color_trans->flXlate & XO_TABLE);
    }

    drawable->effect = QXL_EFFECT_BLEND;
    PushDrawable(pdev, drawable);
    DEBUG_PRINT((pdev, 4, "%s: done\n", __FUNCTION__));

    return TRUE;

punt:
    return EngTransparentBlt(dest, src, clip, color_trans, dest_rect, src_rect, trans_color,
                             reserved);
}

