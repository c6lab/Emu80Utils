/*
 *  rkdisk
 *  (c) Viktor Pykhonin <pyk@mail.ru>, 2024
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// https://github.com/vpyk/EmuUtils


#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string.h>

#include "rkimage/rkvolume.h"


#define VERSION "1.02"


#pragma pack(push, 1)

struct RkHeader {
    uint8_t loadAddrHi;
    uint8_t loadAddrLo;
    uint8_t endAddrHi;
    uint8_t endAddrLo;
};

struct RkFooter {
    uint8_t nullByte1;
    uint8_t nullByte2;
    uint8_t syncByte;
    uint8_t csHi;
    uint8_t csLo;
};

#pragma pack(pop)


#define CP_KOI8     0
#define CP_WIN1251  1
#define CP_UTF8     2
#define CP_MAX_ID   2

const uint16_t koi8_to[CP_MAX_ID][31] = {
    {
        // koi8 -> win1251
        0xDE, 0xC0, 0xC1, 0xD6, 0xC4, 0xC5, 0xD4, 0xC3,
        0xD5, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE,
        0xCF, 0xDF, 0xD0, 0xD1, 0xD2, 0xD3, 0xC6, 0xC2,
        0xDC, 0xDB, 0xC7, 0xD8, 0xDD, 0xD9, 0xD7
    }, {
        // koi8 -> utf8
        0xD0AE, 0xD090, 0xD091, 0xD0A6, 0xD094, 0xD095, 0xD0A4, 0xD093,
        0xD0A5, 0xD098, 0xD099, 0xD09A, 0xD09B, 0xD09C, 0xD09D, 0xD09E,
        0xD09F, 0xD0AF, 0xD0A0, 0xD0A1, 0xD0A2, 0xD0A3, 0xD096, 0xD092,
        0xD0AC, 0xD0AB, 0xD097, 0xD0A8, 0xD0AD, 0xD0A9, 0xD0A7
    }
};

using namespace std;


bool strcmpi(const std::string& a, const char* b)
{
    return (strcasecmp(a.data(), b) == 0);
}


uint16_t addToRkCs(uint16_t baseCs, const uint8_t* data, int len, bool lastChunk = false)
{
    if (lastChunk)
        --len;

    for (int i = 0; i < len; i++) {
        baseCs += data[i];
        baseCs += (data[i] << 8);
    }

    if (lastChunk)
        baseCs = (baseCs & 0xff00) | ((baseCs + data[len]) & 0xff);

    return baseCs;
}


uint16_t calcRkCs(vector<uint8_t>& data)
{
    return addToRkCs(0, data.data(), data.size(), true);
}


bool convertToRk(vector<uint8_t> body, uint16_t loadAddr, const string& outputFile)
{
    int endAddr = loadAddr + body.size() - 1;

    int headerSize = 0;
    int footerSize = 0;

    RkHeader header;
    RkFooter footer;

    const char* headerPtr = (const char*)&header;
    const char* footerPtr = (const char*)&footer;

    headerSize = sizeof(RkHeader);
    header.loadAddrHi = loadAddr >> 8;
    header.loadAddrLo = loadAddr & 0xFF;
    header.endAddrHi = endAddr >> 8;
    header.endAddrLo = endAddr & 0xFF;

    uint16_t cs = calcRkCs(body);

    footerSize = sizeof(RkFooter);
    footer.nullByte1 = 0;
    footer.nullByte2 = 0;
    footer.syncByte = 0xE6;
    footer.csHi = cs >> 8;
    footer.csLo = cs & 0xFF;

    ofstream f(outputFile, ofstream::binary);
    if (f.fail())
        return false;
    f.write(headerPtr, headerSize);
    f.write((const char*)(body.data()), body.size());
    f.write(footerPtr, footerSize);
    if (f.fail()) {
        f.close();
        return false;
    }
    f.close();

    return true;
}


void showTitle(void)
{
    cout << "rkdisk v. " VERSION " (c) Viktor Pykhonin, 2024" << endl;
    cout << "Cyberdyne Systems forked (c) GTU" << endl << endl;
}

void usage(const string& moduleName, bool showtitle = true)
{
            if (showtitle) showTitle();
            cout << "Usage: " << moduleName << " <command> [<options>...] <image_file.rdi> [<rk_file>] [<target_file>]" << endl << endl <<
                    "Commands:" << endl << endl <<
                    "    a   Add file to image" << endl <<
                    "        options:" << endl <<
                    "            -a addr - starting Address (hex), default = 0000" << endl <<
                    "            -o      - Overwrite file if exists" << endl <<
                    "            -r      - set \"Read only\" attribute" << endl <<
                    "            -h      - set \"Hidden\" attribute" << endl <<
                    "    x   eXtract file from image" << endl <<
                    "            -t      - tape (.rk) file pack" << endl <<
                    "            -cp Cxx - codepage text encode (KOI8-R|CP1251|UTF-8)" << endl <<
                    "    d   Delete file from image" << endl <<
                    "    l   List files in image" << endl <<
                    "        options:" << endl <<
                    "            -b - Brief listing" << endl <<
                    "            -b2 - True brief listing ;-)" << endl <<
                    "    f   Format or create new empty image" << endl <<
                    "        options:" << endl <<
                    "            -y      - don't ask to confirm" << endl <<
                    "            -s size - directory Size in sectors (default 4)" << endl <<
                    "    t   set file aTtributes" << endl <<
                    "        options:" << endl <<
                    "            -r      - set \"Read only\" attribute" << endl <<
                    "            -h      - set \"Hidden\" attribute" << endl <<
                    endl;
}


string makeRkDosFileName(const string& rkFileName)
{
    // cut off the extention if any
    int periodPos = rkFileName.find_last_of('.');

    string rkDosName = rkFileName.substr(0, min(periodPos, 10));
    string ext = rkFileName.substr(periodPos + 1, 3);

    if (!ext.empty()) {
        rkDosName.append(".");
        rkDosName.append(ext);
    }

    for (char& ch: rkDosName) {
        if (!(ch >= '0' && ch <= '9') && !(ch >= 'A' && ch <= 'Z') && !(ch >= 'a' && ch <= 'z') && ch != ' ' && ch != '.')
            ch = '_';
    }

    return rkDosName;
}


void listFiles(const string& imageFileName, int briefMode)
{
    RkVolume vol(imageFileName, IFM_READ_ONLY);

    auto fileList = vol.getFileList();
    bool briefListing = (briefMode == 1)? true : false;
    bool b2riefListing = (briefMode == 2)? true : false;

    if (briefListing) {
        int i = 0;
        for (const auto& fi: *fileList) {
            cout << left << setw(14) << setfill(' ') << fi.fileName << "\t";
            if (++i % 5 == 0)
                cout << endl;
        }
        cout << endl;
    } else if (b2riefListing) {
        for (const auto& fi: *fileList) {
            cout << fi.fileName << endl;
        }
    } else {
        cout << "Name          " << "\t" << "Addr" << "\t" << "Blocks" << "\t" << "  Bytes" << "\t" << "  Attr" << endl;
        cout << "----          " << "\t" << "----" << "\t" << "------" << "\t" << "  -----" << "\t" << "  ----" << endl;

        for (const auto& fi: *fileList) {
            string attr = fi.attr & 0x80 ? "R" : "";
            if (fi.attr & 0x40)
                attr += "H";
            cout << left << setw(14) << setfill(' ') << fi.fileName << "\t"
                 << right << setw(4) << setfill('0') << hex << fi.addr << "\t"
                 << setw(6) << setfill(' ') << dec << fi.sCount << "\t"
                 << setw(7) << setfill(' ') << dec << fi.fileSize << "\t"
                 << setw(6) << attr << endl;
        }
    }

    if (!b2riefListing) {
        cout << endl << fileList->size() << " file(s) total" << endl;
        int freeBlocks = vol.getFreeBlocks();
        int freeDirEntries = vol.getFreeDirEntries();
        cout << endl << freeBlocks << " block(s) (" << freeBlocks * 512 << " bytes) free" << endl;
        cout << freeDirEntries << " directory entries free" << endl;
    }
}


void deleteFile(const string& imageFileName, const string& rkFileName)
{
    RkVolume vol(imageFileName, IFM_READ_WRITE);
    vol.deleteFile(rkFileName);
    vol.saveImage();
}


void setAttributes(const string& imageFileName, const string& rkFileName, bool readOnly, bool hidden)
{
    RkVolume vol(imageFileName, IFM_READ_WRITE);
    uint8_t attr = (readOnly ? 0x80 : 0) | (hidden ? 0x40 : 0);
    vol.setAttributes(rkFileName, attr);
    vol.saveImage();
}


bool addFile(const string& imageFileName, const string& rkFileName, uint16_t addr, bool readOnly, bool hidden, bool allowOverwrite)
{
    ifstream rkFile(rkFileName, ios::binary);
    if (!rkFile.is_open()) {
        cout << "error opening file " << rkFileName << endl;
        return false;
    }

    rkFile.seekg(0, ios::end);
    int size = rkFile.tellg();
    rkFile.seekg(0, ios::beg);
    uint8_t* buf = new uint8_t[size];
    rkFile.read((char*)buf, size);

    if (rkFile.rdstate()) {
        cout << "error reading file " << rkFileName << endl;
        delete[] buf;
        return false;
    }

    rkFile.close();

    RkVolume vol(imageFileName, IFM_READ_WRITE);

    uint8_t attr = (readOnly ? 0x80 : 0) | (hidden ? 0x40 : 0);

    vol.writeFile(rkFileName, buf, size, addr, attr, allowOverwrite);
    vol.saveImage();

    return true;
}

vector<uint8_t> decodeCP(const vector<uint8_t>& src, int codePage)
{
    vector<uint8_t> dst;
    uint16_t wch;
    uint8_t ch;

    for (unsigned i = 0; i < src.size(); i++) {
        ch = src[i];
        if (ch == 0x0d) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
            dst.push_back(ch);
#endif
            dst.push_back(0x0a);
        } else {
            if ((ch < 0x60) || (ch == 0x7f));
            else if (ch < 0x7f) {
                ch += 0x80;
                if (codePage > CP_KOI8) {
                    wch = koi8_to[codePage - 1][ch - 0xE0];
                    ch = wch & 0xFF;
                    if (wch & 0xFF00) dst.push_back((wch & 0xFF00) >> 8);
                }
            }
            else if (ch == 0xff) continue; //ch = 0x1a;
            dst.push_back(ch);
        }
    }

    return dst;
}

bool extractFile(const string& imageFileName, const string& rkFileName, const string& targetFileName, bool extractToTape, int codePage)
{
    RkVolume vol(imageFileName, IFM_READ_ONLY);

    int size = 0;
    uint16_t start = 0;
    uint8_t* buf = vol.readFile(rkFileName, size, start);
    if ((size == 0) || (buf == nullptr)) {
        cout << "The file size is zero " << rkFileName << endl;
        return false;
    }
    // fix--
    vector<uint8_t> body(size);
    memcpy(body.data(), buf, size);
    delete[] buf;
    // --fix

    if (codePage >= CP_KOI8) {
        body = decodeCP(body, codePage);
    }

    if (!extractToTape) {
        ofstream rkFile(targetFileName, ios::binary | std::fstream::trunc);
        if (!rkFile.is_open()) {
            cout << "error opening file " << rkFileName << endl;
            return false;
        }
        rkFile.write(reinterpret_cast<char*>(body.data()), body.size());
        if (rkFile.rdstate()) {
            cout << "error writing file " << rkFileName << endl;
            return false;
        }
        rkFile.close();
        return true;
    } else {
        return convertToRk(body, start, targetFileName);
    }
}


void formatImage(const string& imageFileName, int directorySize)
{
    RkVolume vol(imageFileName, IFM_WRITE_CREATE);
    vol.format(directorySize);
    vol.saveImage();
}


int main(int argc, const char** argv)
{
    string moduleName = argv[0];
    moduleName = moduleName.substr(moduleName.find_last_of("/\\:") + 1);

    string imageFileName;
    string rkFileName;
    string targetFileName;
    string cp_str;

    bool allowOverwrite = false;
    bool briefListing = false;
    bool b2riefListing = false;
    bool readOnly = false;
    bool hidden = false;
    bool noConfirmation = false;
    bool extractToTape = false;
    uint16_t startingAddr = 0;
    int directorySize = 4;
    int codePage = -1;

    // parse command line

    if (argc < 2) {
        usage(moduleName);
        return 1;
    }

    string command = argv[1];
    int i = 2;

    string option, value;
    while (i < argc) {
        option = argv[i];

        if (option == "-o") {
            if (i > argc || command != "a") {
                usage(moduleName);
                return 1;
            }
            allowOverwrite = true;
        } else if (option == "-a") {
            ++i;
            if (i > argc || command != "a") {
                usage(moduleName);
                return 1;
            }
            value = argv[i];

            char* numEnd;
            startingAddr = strtoul(value.c_str(), &numEnd, 16);
            if (*numEnd) {
                cout << "Invalid starting address!" << endl << endl;
                usage(moduleName);
                return 1;
            }
        } else if (option == "-s") {
            ++i;
            if (i > argc || command != "f") {
                usage(moduleName);
                return 1;
            }
            value = argv[i];

            char* numEnd;
            directorySize = strtoul(value.c_str(), &numEnd, 10);
            if (*numEnd || directorySize < 1 || directorySize > 99) {
                cout << "Invalid directory size!" << endl << endl;
                usage(moduleName);
                return 1;
            }
        } else if (option == "-b") {
            if (i > argc || command != "l") {
                usage(moduleName);
                return 1;
            }
            briefListing = true;
        } else if (option == "-b2") {
            if (i > argc || command != "l") {
                usage(moduleName);
                return 1;
            }
            b2riefListing = true;
        } else if (option == "-y") {
            if (i > argc || command != "f") {
                usage(moduleName);
                return 1;
            }
            noConfirmation = true;
        } else if (option == "-r") {
            if (i > argc || (command != "a" && command != "t")) {
                usage(moduleName);
                return 1;
            }
            readOnly = true;
        } else if (option == "-h") {
            if (i > argc || (command != "a" && command != "t")) {
                usage(moduleName);
                return 1;
            }
            hidden = true;
        } else if (option == "-t") {
            if (i > argc || command != "x") {
                usage(moduleName);
                return 1;
            }
            extractToTape = true;
        } else if (option == "-cp") {
            if (++i > argc || command != "x") {
                usage(moduleName);
                return 1;
            }
            cp_str = argv[i];
            if (strcmpi(cp_str, "KOI8-R")) codePage = CP_KOI8;
            else if (strcmpi(cp_str, "CP1251")) codePage = CP_WIN1251;
            else if (strcmpi(cp_str, "UTF-8")) codePage = CP_UTF8;
            else {
                usage(moduleName);
                return 1;
            }
        } else {
            if (option[0] == '-') {
                cout << "Invalid option:" << option << endl << endl;
                usage(moduleName);
                return 1;
            }

            if (imageFileName.empty()) {
                imageFileName = option;
            } else if (rkFileName.empty()) {
                rkFileName = option;
            } else if (targetFileName.empty()) {
                targetFileName = option;
            } else {
                usage(moduleName);
                return 1;
            }
        }

        ++i;
    }

    if (command != "a" && command != "x" && command != "d" && command != "l" && command != "f" && command != "t") {
        cout << "Unknown comamnd \"" << command << "\"" << endl << endl;
        usage(moduleName);
        return 1;
    }

    if (imageFileName.empty()) {
        cout << "No image file name specified!" << endl << endl;
        usage(moduleName);
        return 1;
    }

    if (!b2riefListing) {
        showTitle();
    }

    try {

        if (command == "l") {
            if (!rkFileName.empty()) {
                cout << "Extra file name specified!" << endl << endl;
                usage(moduleName, b2riefListing);
                return 1;
            }
            if (!b2riefListing) {
                cout << "Directory content for image " << imageFileName << ":" << endl << endl;
            }
            listFiles(imageFileName, briefListing? 1 : b2riefListing? 2 : 0);
            return 0;
        } else if (command == "f") {
            if (!targetFileName.empty()) {
                cout << "Extra file name specified!" << endl << endl;
                usage(moduleName, b2riefListing);
                return 1;
            }
            if (!noConfirmation) {
                cout << "Format image " << imageFileName << "? [y/N] ";
                char ch;
                cin.get(ch);
                if (ch != 'y' && ch != 'Y')
                    return 1;
            }

            cout << "Formatting image " << imageFileName << ", " << directorySize << " sector(s) directory ... ";
            formatImage(imageFileName, directorySize);
            cout << "done." << endl;
            return 0;
        }

        if (rkFileName.empty()) {
            cout << "No rk file name specified!" << endl << endl;
            usage(moduleName, b2riefListing);
            return 1;
        }

        if (command == "x") {
            if (targetFileName.empty())
                targetFileName = rkFileName;
            cout << "Extracting file " << rkFileName << " from image " << imageFileName << " to " << targetFileName << " ... ";
            if (!extractFile(imageFileName, rkFileName, targetFileName, extractToTape, codePage))
                return 1;
        } else if (command == "a") {
            if (!targetFileName.empty()) {
                cout << "Extra file name specified!" << endl << endl;
                usage(moduleName, b2riefListing);
                return 1;
            }
            string rkFileNameWoPath = rkFileName.substr(rkFileName.find_last_of("/\\:") + 1);
            string newRkFileName = makeRkDosFileName(rkFileNameWoPath);
            if (rkFileNameWoPath != newRkFileName)
                cout << "New rk file name: " << newRkFileName << endl;
            cout << "Adding file " << rkFileNameWoPath << " to image " << imageFileName << " ... ";
            addFile(imageFileName, newRkFileName, startingAddr, readOnly, hidden, allowOverwrite);
        } else if (command == "d") {
            if (!targetFileName.empty()) {
                cout << "Extra file name specified!" << endl << endl;
                usage(moduleName, b2riefListing);
                return 1;
            }
            cout << "Deleting file " << rkFileName << " from image " << imageFileName << " ... ";
            deleteFile(imageFileName, rkFileName);
        } else if (command == "t") {
            if (!targetFileName.empty()) {
                cout << "Extra file name specified!" << endl << endl;
                usage(moduleName, b2riefListing);
                return 1;
            }
            cout << "Setting file attributes " << rkFileName << " from image " << imageFileName << " ... ";
            setAttributes(imageFileName, rkFileName, readOnly, hidden);
        }

        cout << "done." << endl;

        return 0;
    }

    catch (RkVolume::RkVolumeException& e) {
        cout << "image error: ";
        switch (e.type) {
        case RkVolume::RkVolumeException::RVET_SECTOR_NOT_FOUND:
            cout << "sector not found! Track " << e.track << ", sector " << e.sector << "." << endl;
            break;
        case RkVolume::RkVolumeException::RVET_DISK_FULL:
            cout << "insufficient disk space!" << endl;
            break;
        case RkVolume::RkVolumeException::RVET_DIR_FULL:
            cout << "No more dir entries!" << endl;
            break;
        case RkVolume::RkVolumeException::RVET_BAD_DISK_FORMAT:
            cout << "bad disk image! " << e.track << ", " << e.sector << endl;
            break;
        case RkVolume::RkVolumeException::RVET_NO_FILESYSTEM:
            cout << "no filesyetem on image!" << endl;
            break;
        case RkVolume::RkVolumeException::RVET_FILE_NOT_FOUND:
            cout << "file not found!" << endl;
            break;
        case RkVolume::RkVolumeException::RVET_FILE_EXISTS:
            cout << "file already exists!" << endl;
            break;
        default:
            cout << "unknown error!" << endl;
        }
    }

    catch (ImageFileException& e) {
        cout << endl << "Disk error: ";
        switch (e) {
        case IFE_OPEN_ERROR:
            cout << "file open error!" << endl;
            break;
        case IFE_READ_ERROR:
            cout << "file read error!" << endl;
            break;
        case IFE_WRITE_ERROR:
            cout << "file write error!" << endl;
            break;
        }
    }

    return 1;
}
