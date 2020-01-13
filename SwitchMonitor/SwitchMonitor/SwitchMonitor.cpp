#include <Windows.h>
#include <lowlevelmonitorconfigurationapi.h>

////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <map>
#include <vector>

////////////////////////////////////////////////////////////////////////////////

struct HexCharStruct
{
    unsigned char c;
    HexCharStruct(unsigned char _c) : c(_c) { }
};

inline std::ostream& operator<<(std::ostream& o, const HexCharStruct& hs)
{
    return (o << std::setw(2) << std::setfill('0') << std::hex << (int)hs.c);
}

inline HexCharStruct hex(unsigned char _c)
{
    return HexCharStruct(_c);
}

////////////////////////////////////////////////////////////////////////////////

const char* testVCPCaps = "(prot(monitor)type(LCD)model(IIYAMA)cmds(01 02 03 07 0C E3 F3)vcp(0(01 02 04 05 06 08 0B) 16 18 1A 52 60(01 03 0F) 62 87 AC AE B2 B6 C6 C8 CA CC(01 02 03 04 06 0A 0D) D6(01 04) DF FD FF)mswhql(1)asset_eep(40)mccs_ver(2.2))";

////////////////////////////////////////////////////////////////////////////////

const uint8_t k_selectActiveVideoSourceCmd = 0x60;

const std::map<uint8_t, std::string> k_inputSourceValues = 
{
    {0x01,  "VGA-1"},    // aka Analog video (R/G/B) 1
    {0x02,  "VGA-2"},
    {0x03,  "DVI-1"},
    {0x04,  "DVI-2"},
    {0x05,  "Composite video 1"},
    {0x06,  "Composite video 2"},
    {0x07,  "S-Video-1"},
    {0x08,  "S-Video-2"},
    {0x09,  "Tuner-1"},
    {0x0a,  "Tuner-2"},
    {0x0b,  "Tuner-3"},
    {0x0c,  "Component video (YPrPb/YCrCb) 1"},
    {0x0d,  "Component video (YPrPb/YCrCb) 2"},
    {0x0e,  "Component video (YPrPb/YCrCb) 3"},
    // end of v2.0 values
    // remaining values in v2.2 spec, assume also valid for v2.1
    {0x0f,  "DisplayPort-1"},
    {0x10,  "DisplayPort-2"},
    {0x11,  "HDMI-1"},
    {0x12,  "HDMI-2"},
    {0x00,  ""}
};

////////////////////////////////////////////////////////////////////////////////

struct MonitorInfo
{
    PHYSICAL_MONITOR physicalMonitor;

    std::string name;
    std::string capabilities;
    std::vector<int8_t> availableSources;
};

std::vector<MonitorInfo> availableMonitors;

////////////////////////////////////////////////////////////////////////////////

void SelectActiveVideoSource(int32_t monitorIndex, uint8_t source)
{
    if (monitorIndex < 0 || availableMonitors.size() < monitorIndex)
    {
        std::cout << "ERROR: monitor index out of range. Use 'SwitchMonitor list' to check available monitors" << std::endl;
        return;
    }

    MonitorInfo mInfo = availableMonitors[monitorIndex];

    if (k_inputSourceValues.find(source) != k_inputSourceValues.end())
    {
        std::cout << "Switching monitor " << monitorIndex << " input source to " << k_inputSourceValues.at(source) << std::endl;
    }
    else
    {
        std::cout << "Warning: switching monitor " << mInfo.physicalMonitor.hPhysicalMonitor << " input source to unknown source id " << source << std::endl;
    }

    SetVCPFeature(mInfo.physicalMonitor.hPhysicalMonitor, k_selectActiveVideoSourceCmd, source);
}

////////////////////////////////////////////////////////////////////////////////

void DestroyAvailableMonitors()
{
    if (!availableMonitors.empty())
    {
        for (const MonitorInfo& mi : availableMonitors)
        {
            DestroyPhysicalMonitor(mi.physicalMonitor.hPhysicalMonitor);
        }

        availableMonitors.clear();
    }
}

////////////////////////////////////////////////////////////////////////////////

std::string GetValue(std::string::iterator input, std::string::iterator end)
{
    auto it = input;
    while (it != end && *it != '(') ++it;

    ++it;
    auto beginIt = it;

    int depth = 1;
    while (depth > 0 && it != end)
    {
        if (*it == '(')
        {
            ++depth;
        }
        else if (*it == ')')
        {
            --depth;
        }

        ++it;
    }

    if (depth > 0)
    {
        return "";
    }

    size_t len = std::distance(beginIt, it);

    return std::string(beginIt, beginIt + len - 1);
}

////////////////////////////////////////////////////////////////////////////////

MonitorInfo ParseMonitor(PHYSICAL_MONITOR phyMon, std::string caps)
{
    MonitorInfo mInfo;
    mInfo.physicalMonitor = phyMon;
    mInfo.capabilities = caps;

    if (!caps.empty())
    {
        std::string::iterator modelIt = caps.begin() + caps.find("model", 0);
        mInfo.name = GetValue(modelIt, caps.end());

        std::string::iterator vcpIt = caps.begin() + caps.find("vcp", 0);
        std::string vcpValue = GetValue(vcpIt, caps.end());

        std::string::iterator x60It = vcpValue.begin() + vcpValue.find("60", 0);
        std::string x60Value = GetValue(x60It, vcpValue.end());

        std::istringstream x60ValueSS(x60Value);
        std::string x60Entry;
        while (std::getline(x60ValueSS, x60Entry, ' '))
        {
            uint8_t x = static_cast<uint8_t>(std::stoul(x60Entry, nullptr, 16));
            mInfo.availableSources.push_back(x);
        }
    }

    return mInfo;
}

////////////////////////////////////////////////////////////////////////////////

void PrintMonitors()
{
    int idx = 0;
    for (MonitorInfo mInfo : availableMonitors)
    {
        std::cout << "Monitor #" << idx << " HANDLE_" << mInfo.physicalMonitor.hPhysicalMonitor << " ";
        std::wcout << mInfo.physicalMonitor.szPhysicalMonitorDescription << std::endl;
        //std::cout << "\tVCP Capabilities: " << mInfo.capabilities << std::endl;
        std::cout << "\tVendor: " << mInfo.name << std::endl;

        std::cout << "\tAvailable sources: " << std::endl;
        for (uint8_t source : mInfo.availableSources)
        {
            auto sit = k_inputSourceValues.find(source);
            if (sit != k_inputSourceValues.end())
            {
                std::cout << "\t\t" << hex(sit->first) << " - " << sit->second << std::endl;
            }
            else
            {
                std::cout << "\t\t" << hex(sit->first) << " - unknown" << std::endl;
            }
        }

        std::cout << std::endl;
        ++idx;
    }

    std::cout << std::endl;
    std::cout << "Input sources:";

    for (auto sourceInfo : k_inputSourceValues)
    {
        std::cout << "\t" << hex(sourceInfo.first) << " - " << sourceInfo.second << std::endl;
    }

    std::cout << std::endl;
}

////////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC hDC, LPRECT lpRect, LPARAM lParam)
{
    DWORD numberOfPhysicalMonitors;
    BOOL bSuccess = GetNumberOfPhysicalMonitorsFromHMONITOR(hMon, &numberOfPhysicalMonitors);

    if (!bSuccess)
    {
        std::cout << "Error: cannot get number of physical monitors from HMONITOR" << std::endl;
        return FALSE;
    }

    std::vector<PHYSICAL_MONITOR> physicalMonitors(numberOfPhysicalMonitors);
    bSuccess = GetPhysicalMonitorsFromHMONITOR(hMon, numberOfPhysicalMonitors, physicalMonitors.data());

    if (!bSuccess)
    {
        std::cout << "Error: cannot get physical monitors from HMONITOR" << std::endl;
        return FALSE;
    }

    if (lParam)
    {
        DWORD capabilitiesStringLength;
        std::vector<char> capabilitiesStringData;
        std::string capabilitiesString;
        for (PHYSICAL_MONITOR phyMon : physicalMonitors)
        {
            bSuccess = GetCapabilitiesStringLength(phyMon.hPhysicalMonitor, &capabilitiesStringLength);

            if (!bSuccess)
            {
                std::cout << "Error: cannot get monitor capabilities string length for " << phyMon.hPhysicalMonitor << std::endl;
                DestroyPhysicalMonitor(phyMon.hPhysicalMonitor);
            }

            capabilitiesStringData.resize(capabilitiesStringLength);

            bSuccess = CapabilitiesRequestAndCapabilitiesReply(phyMon.hPhysicalMonitor, capabilitiesStringData.data(), capabilitiesStringLength);

            if (!bSuccess)
            {
                std::cout << "Error: cannot get monitor capabilities string for " << phyMon.hPhysicalMonitor << std::endl;
                DestroyPhysicalMonitor(phyMon.hPhysicalMonitor);
            }

            capabilitiesString = std::string(capabilitiesStringData.begin(), capabilitiesStringData.end());
            availableMonitors.push_back(ParseMonitor(phyMon, capabilitiesString));
        }
    }
    else
    {
        for (PHYSICAL_MONITOR phyMon : physicalMonitors)
        {
            availableMonitors.push_back(ParseMonitor(phyMon, std::string()));
        }
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

void GetAllMonitors(bool getCaps)
{
    DestroyAvailableMonitors();
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, getCaps ? 1 : 0);
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    ++argv;
    --argc;

    if (argc == 1 && strcmp(argv[0], "list") == 0)
    {
        GetAllMonitors(true);
        PrintMonitors();
    }
    else if (argc == 3 && strcmp(argv[0], "switch") == 0)
    {
        GetAllMonitors(false);

        int32_t monitorIndex = std::stoi(argv[1]);
        uint32_t source = static_cast<uint8_t>(std::stoul(argv[2], nullptr, 16));

        SelectActiveVideoSource(monitorIndex, source);
    }
    else
    {
        std::cout << std::endl;
        std::cout << "Use 'SwitchMonitor list' to print all available monitors and input source codes" << std::endl;
        std::cout << "Use 'SwitchMonitor switch <monitorIndex> <inputSourceCode HEX VALUE>' to switch monitor input source" << std::endl;
        std::cout << std::endl;
    }

    DestroyAvailableMonitors();
}

////////////////////////////////////////////////////////////////////////////////
