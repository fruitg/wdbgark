/*
    * WinDBG Anti-RootKit extension
    * Copyright � 2013-2016  Vyacheslav Rusakoff
    * 
    * This program is free software: you can redistribute it and/or modify
    * it under the terms of the GNU General Public License as published by
    * the Free Software Foundation, either version 3 of the License, or
    * (at your option) any later version.
    * 
    * This program is distributed in the hope that it will be useful,
    * but WITHOUT ANY WARRANTY; without even the implied warranty of
    * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    * GNU General Public License for more details.
    * 
    * You should have received a copy of the GNU General Public License
    * along with this program.  If not, see <http://www.gnu.org/licenses/>.

    * This work is licensed under the terms of the GNU GPL, version 3.  See
    * the COPYING file in the top-level directory.
*/

/*
    nt!HalDispatchTable, nt!HalPrivateDispatchTable, nt!HalIommuDispatchTable
*/

#include <sstream>
#include <map>

#include "wdbgark.hpp"
#include "analyze.hpp"
#include "manipulators.hpp"

namespace wa {
//////////////////////////////////////////////////////////////////////////
typedef struct HalDispatchTableInfoTag {
    uint8_t hdt_count;      // HalDispatchTable table count
    uint8_t hpdt_count;     // HalPrivateDispatchTable table count
    uint8_t hiommu_count;   // HalIommuDispatch table count (W8.1+)
    uint8_t skip;           // Skip first N entries
} HalDispatchTableInfo;

using HalTableInfo = std::map<uint32_t, HalDispatchTableInfo>;
//////////////////////////////////////////////////////////////////////////
HalTableInfo GetHalTableInfo();
//////////////////////////////////////////////////////////////////////////
EXT_COMMAND(wa_haltables, "Output kernel-mode HAL tables: "\
            "nt!HalDispatchTable, nt!HalPrivateDispatchTable, nt!HalIommuDispatchTable", "") {
    RequireKernelMode();

    if ( !Init() )
        throw ExtStatusException(S_OK, "global init failed");

    out << wa::showplus << "Displaying HAL tables" << endlout;

    if ( !m_system_ver->IsInited() ) {
        out << wa::showplus << __FUNCTION__ << ": unsupported Windows version" << endlout;
        return;
    }

    auto hal_tbl_info = GetHalTableInfo();
    auto citer = hal_tbl_info.find(m_system_ver->GetStrictVer());

    if ( citer == hal_tbl_info.end() ) {
        err << wa::showminus << __FUNCTION__ << ": unable to correlate internal info with the minor build" << endlerr;
        return;
    }

    uint64_t offset_hdt = 0;
    uint64_t offset_hpdt = 0;
    uint64_t offset_hiommu = 0;

    if ( !m_sym_cache->GetSymbolOffset("nt!HalDispatchTable", true, &offset_hdt) ) {
        err << wa::showminus << __FUNCTION__ << ": failed to find nt!HalDispatchTable" << endlerr;
    }

    if ( !m_sym_cache->GetSymbolOffset("nt!HalPrivateDispatchTable", true, &offset_hpdt) ) {
        err << wa::showminus << __FUNCTION__ << ": failed to find nt!HalPrivateDispatchTable" << endlerr;
    }

    if ( m_system_ver->GetStrictVer() >= W81RTM_VER
         &&
         !m_sym_cache->GetSymbolOffset("nt!HalIommuDispatchTable", true, &offset_hiommu) ) {
        err << wa::showminus << __FUNCTION__ << ": failed to find nt!HalIommuDispatchTable" << endlerr;
    }

    auto display = WDbgArkAnalyzeBase::Create(m_sym_cache);

    try {
        walkresType output_list_hdt;
        walkresType output_list_hpdt;
        walkresType output_list_hiommu;

        if ( offset_hdt ) {
            WalkAnyTable(offset_hdt,
                         citer->second.skip * m_PtrSize,
                         citer->second.hdt_count,
                         m_PtrSize,
                         "",
                         &output_list_hdt,
                         false,
                         true);
        }

        if ( offset_hpdt ) {
            WalkAnyTable(offset_hpdt,
                         citer->second.skip * m_PtrSize,
                         citer->second.hpdt_count,
                         m_PtrSize,
                         "",
                         &output_list_hpdt,
                         false,
                         true);
        }

        if ( offset_hiommu ) {
            WalkAnyTable(offset_hiommu,
                         0,
                         citer->second.hiommu_count,
                         m_PtrSize,
                         "",
                         &output_list_hiommu,
                         false,
                         true);
        }

        out << wa::showplus << "nt!HalDispatchTable: " << std::hex << std::showbase << offset_hdt << endlout;
        display->PrintHeader();

        for ( const auto &walk_info : output_list_hdt ) {
            display->Analyze(walk_info.address, walk_info.type, walk_info.info);
            display->PrintFooter();
        }

        out << wa::showplus << "nt!HalPrivateDispatchTable: " << std::hex << std::showbase << offset_hpdt << endlout;
        display->PrintHeader();

        for ( const auto &walk_info : output_list_hpdt ) {
            display->Analyze(walk_info.address, walk_info.type, walk_info.info);
            display->PrintFooter();
        }

        if ( m_system_ver->GetStrictVer() >= W81RTM_VER ) {
            out << wa::showplus << "nt!HalIommuDispatchTable: ";
            out << std::hex << std::showbase << offset_hiommu << endlout;
            display->PrintHeader();

            for ( const auto &walk_info : output_list_hiommu ) {
                display->Analyze(walk_info.address, walk_info.type, walk_info.info);
                display->PrintFooter();
            }
        }
    }
    catch( const ExtInterruptException& ) {
        throw;
    }

    display->PrintFooter();
}

HalTableInfo GetHalTableInfo() {
    return { {
        { WXP_VER, { 0x15, 0x12, 0x0, 0x1 } },
        { W2K3_VER, { 0x15, 0x13, 0x0, 0x1 } },
        { VISTA_RTM_VER, { 0x16, 0x1B, 0x0, 0x1 } },
        { VISTA_SP1_VER, { 0x18, 0x22, 0x0, 0x1 } },
        { VISTA_SP2_VER, { 0x17, 0x23, 0x0, 0x1 } },
        { W7RTM_VER, { 0x16, 0x2D, 0x0, 0x1 } },
        { W7SP1_VER, { 0x16, 0x2D, 0x0, 0x1 } },
        { W8RTM_VER, { 0x16, 0x5A, 0x0, 0x1 } },
        { W81RTM_VER, { 0x16, 0x69, 0x0B, 0x1 } },
        { W10RTM_VER, { 0x16, 0x71, 0x10, 0x1 } },
        { W10TH2_VER, { 0x16, 0x71, 0x10, 0x1 } },
        { W10RS1_VER, { 0x16, 0x78, 0x11, 0x1 } }
        } };
}

}   // namespace wa
