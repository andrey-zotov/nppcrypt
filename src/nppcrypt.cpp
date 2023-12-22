/*
This file is part of nppcrypt
(http://www.github.com/jeanpaulrichter/nppcrypt)
a plugin for notepad++ [ Copyright (C)2003 Don HO <don.h@free.fr> ]
(https://notepad-plus-plus.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include "npp/Definitions.h"
#include "nppcrypt.h"
#include "exception.h"
#include "preferences.h"
#include "crypt.h"
#include "crypt_help.h"
#include "dlg_crypt.h"
#include "dlg_hash.h"
#include "dlg_random.h"
#include "dlg_about.h"
#include "dlg_preferences.h"
#include "dlg_auth.h"
#include "dlg_convert.h"
#include "dlg_initdata.h"
#include "messagebox.h"
#include "cryptheader.h"
#include "resource.h"
#include "help.h"

typedef std::map<std::wstring, CryptInfo> cryptfilemap;
enum class NppCryptFileSave : unsigned { DEFAULT, OLD_ENCRYPTION, NO_ENCRYPTION };

const TCHAR             NPP_PLUGIN_NAME[] = TEXT(NPPC_NAME);
const int               NPPCRYPT_VERSION = NPPC_VERSION;

FuncItem                funcItem[NPPC_FUNC_COUNT];
NppData                 nppData;
HINSTANCE               m_hInstance;
CurrentOptions          current;
cryptfilemap            crypt_files;
NppCryptFileSave        crypt_file_cursave;
UINT_PTR                last_known_scintilla_first_line = 0;
UINT_PTR                last_known_scintilla_pos = 0;
UINT_PTR                last_known_scintilla_anchor = 0;

DlgCrypt                dlg_crypt;
DlgHash                 dlg_hash(current.hash);
DlgRandom               dlg_random(current.random);
DlgAuth                 dlg_auth;
DlgPreferences          dlg_preferences;
DlgAbout                dlg_about;
DlgConvert              dlg_convert(current.convert);
DlgInitdata             dlg_initdata;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD reasonForCall, LPVOID lpReserved )
{
    switch (reasonForCall)
    {
    case DLL_PROCESS_ATTACH:
    {
        #ifdef _DEBUG
            _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
        #endif
        m_hInstance = (HINSTANCE)hModule;
        break;
    }
    case DLL_PROCESS_DETACH:
    {
        preferences.save(current);
        dlg_random.destroy();
        dlg_hash.destroy();
        dlg_crypt.destroy();
        dlg_about.destroy();
        dlg_preferences.destroy();
        dlg_auth.destroy();
        dlg_convert.destroy();
        dlg_initdata.destroy();
        break;
    }
    case DLL_THREAD_ATTACH:
    {
        break;
    }
    case DLL_THREAD_DETACH:
    {
        break;
    }
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
    nppData = notpadPlusData;

    help::npp::setCommand(0, TEXT("Encrypt"), EncryptDlg, NULL, false);
    help::npp::setCommand(1, TEXT("Decrypt"), DecryptDlg, NULL, false);
    help::npp::setCommand(2, TEXT("Hash"), HashDlg, NULL, false);
    help::npp::setCommand(3, TEXT("Random"), RandomDlg, NULL, false);
    help::npp::setCommand(4, TEXT("Convert"), ConvertDlg, NULL, false);
    help::npp::setCommand(5, TEXT("---"), NULL, NULL, false);
    help::npp::setCommand(6, TEXT("Preferences"), PreferencesDlg, NULL, false);
    help::npp::setCommand(7, TEXT("---"), NULL, NULL, false);
    help::npp::setCommand(8, TEXT("About"), AboutDlg, NULL, false);

    dlg_random.init(m_hInstance, nppData._nppHandle, IDD_RANDOM, IDC_OK);
    dlg_hash.init(m_hInstance, nppData._nppHandle, IDD_HASH, IDC_OK);
    dlg_crypt.init(m_hInstance, nppData._nppHandle, IDD_CRYPT, IDC_OK);
    dlg_about.init(m_hInstance, nppData._nppHandle, IDD_ABOUT, IDC_OK);
    dlg_preferences.init(m_hInstance, nppData._nppHandle, IDD_PREFERENCES, IDC_OK);
    dlg_auth.init(m_hInstance, nppData._nppHandle, IDD_AUTH, IDC_OK);
    dlg_convert.init(m_hInstance, nppData._nppHandle, IDD_CONVERT, IDC_OK);
    dlg_initdata.init(m_hInstance, nppData._nppHandle, IDD_INITDATA, IDC_OK);

    TCHAR preffile_s[MAX_PATH];
    ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)preffile_s);
    int preffile_len = lstrlen(preffile_s);
    if (preffile_len + 16 < MAX_PATH) {
        lstrcpy(preffile_s + preffile_len, TEXT("\\nppcrypt.xml"));
        preferences.load(preffile_s, current);
    }
}

extern "C" __declspec(dllexport) BOOL isUnicode()
{
    return TRUE;
}

extern "C" __declspec(dllexport) const TCHAR * getName()
{
    return NPP_PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF)
{
    *nbF = int(NPPC_FUNC_COUNT);
    return funcItem;
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    return TRUE;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
    switch (notifyCode->nmhdr.code)
    {
    case NPPN_FILEOPENED:
    {
        if (!preferences.files.enable) {
            return;
        }
        try {
            std::wstring path, filename, extension;
            help::buffer::getPath(notifyCode->nmhdr.idFrom, path, filename, extension);

            if(preferences.files.extension.compare(extension) == 0) {
                ::SendMessage(nppData._nppHandle, NPPM_SWITCHTOFILE, 0, (LPARAM)path.c_str());
                HWND hCurScintilla = help::scintilla::getCurrent();

                int data_length = (int)::SendMessage(hCurScintilla, SCI_GETLENGTH , 0, 0);
                if (data_length <= 0) {
                    throwInfo(file_empty);
                }
                byte* pData = (byte*)::SendMessage(hCurScintilla, SCI_GETCHARACTERPOINTER, 0, 0);
                if (!pData) {
                    throwError(no_scintilla_pointer);
                }

                CryptInfo               crypt;
                nppcrypt::InitData         initdata;
                CryptHeaderReader       header(crypt.hmac);

                if (!header.parse(crypt.options, initdata, pData, data_length)) {
                    throwInvalid(missing_header);
                }

                if(crypt.hmac.enable) {
                    if (crypt.hmac.keypreset_id < 0) {
                        if (!dlg_auth.doDialog()) {
                            return;
                        }
                        crypt.hmac.hash.key.set(dlg_auth.getInput());
                        if (!header.checkHMAC()) {
                            throwInfo(hmac_auth_failed);
                        }
                    } else {
                        if (crypt.hmac.keypreset_id >= preferences.getKeyNum()) {
                            throwInvalid(invalid_keypreset_id);
                        } else {
                            crypt.hmac.hash.key.set(preferences.getKey(crypt.hmac.keypreset_id), 16);
                            if (!header.checkHMAC()) {
                                throwInfo(hmac_auth_failed);
                            }
                        }
                    }
                }

                if (dlg_crypt.decryptDialog(&crypt, &initdata.iv, &filename)) {
                    std::basic_string<byte> buffer;
                    nppcrypt::decrypt(header.getEncrypted(), header.getEncryptedLength(), buffer, crypt.options, crypt.password, initdata);

                    ::SendMessage(hCurScintilla, SCI_CLEARALL, 0, 0);
                    ::SendMessage(hCurScintilla, SCI_APPENDTEXT, buffer.size(), (LPARAM)&buffer[0]);
                    ::SendMessage(hCurScintilla, SCI_GOTOPOS, 0, 0);
                    ::SendMessage(hCurScintilla, SCI_EMPTYUNDOBUFFER, 0, 0);
                    ::SendMessage(hCurScintilla, SCI_SETSAVEPOINT, 0, 0);

                    crypt_files.insert(std::pair<std::wstring, CryptInfo>(path, crypt));
                }
            }
        } catch (ExcInfo& exc) {
            if (exc.getID() != ExcInfo::ID::file_empty) {
                msgbox::info(nppData._nppHandle, exc.what(), exc.getURL(), exc.getURLCaption());
            }
        } catch(std::exception& exc) {
            msgbox::error(nppData._nppHandle, exc.what());
        } catch(...) {
            msgbox::error(nppData._nppHandle, "unknown exception!");
        }
        break;
    }
    case NPPN_FILESAVED:
    {
        try {
            if (!preferences.files.enable) {
                return;
            }
            std::wstring path, filename, extension;
            help::buffer::getPath(notifyCode->nmhdr.idFrom, path, filename, extension);

            if (preferences.files.extension.compare(extension) == 0) {

                if (crypt_file_cursave != NppCryptFileSave::NO_ENCRYPTION) {
                    HWND hCurScintilla = help::scintilla::getCurrent();
                    ::SendMessage(hCurScintilla, SCI_UNDO, 0, 0);
                    ::SendMessage(hCurScintilla, SCI_SETANCHOR, last_known_scintilla_anchor, 0);
                    ::SendMessage(hCurScintilla, SCI_SETCURRENTPOS, last_known_scintilla_pos, 0);
                    ::SendMessage(hCurScintilla, SCI_SETFIRSTVISIBLELINE, last_known_scintilla_first_line, last_known_scintilla_first_line);
                    ::SendMessage(hCurScintilla, SCI_EMPTYUNDOBUFFER, 0, 0);
                    ::SendMessage(hCurScintilla, SCI_SETSAVEPOINT, 0, 0);
                }
                if (crypt_file_cursave == NppCryptFileSave::OLD_ENCRYPTION) {
                    msgbox::info(nppData._nppHandle, "The file was saved using the old encryption!");
                } else if (crypt_file_cursave == NppCryptFileSave::NO_ENCRYPTION) {
                    msgbox::error(nppData._nppHandle, "The file was saved without encryption!");
                }
            }
        } catch (ExcInfo& exc) {
            if (exc.getID() != ExcInfo::ID::file_empty) {
                msgbox::info(nppData._nppHandle, exc.what(), exc.getURL(), exc.getURLCaption());
            }
        } catch (std::exception& exc) {
            msgbox::error(nppData._nppHandle, exc.what());
        } catch (...) {
            msgbox::error(nppData._nppHandle, "unknown exception!");
        }
        break;
    }
    case NPPN_FILEBEFORESAVE:
    {
        try {
            if (!preferences.files.enable) {
                return;
            }
            std::wstring path, filename, extension;
            help::buffer::getPath(notifyCode->nmhdr.idFrom, path, filename, extension);

            if (preferences.files.extension.compare(extension) == 0) {

                cryptfilemap::iterator  fiter = crypt_files.find(path);
                bool                    old_encryption_available = (fiter != crypt_files.end());
                CryptInfo& crypt = old_encryption_available ? fiter->second : current.crypt;

                ::SendMessage(nppData._nppHandle, NPPM_SWITCHTOFILE, 0, (LPARAM)path.c_str());
                HWND hCurScintilla = help::scintilla::getCurrent();

                last_known_scintilla_first_line = (UINT_PTR)::SendMessage(hCurScintilla, SCI_GETFIRSTVISIBLELINE, 0, 0);
                last_known_scintilla_pos = (UINT_PTR)::SendMessage(hCurScintilla, SCI_GETCURRENTPOS, 0, 0);
                last_known_scintilla_anchor = (UINT_PTR)::SendMessage(hCurScintilla, SCI_GETANCHOR, 0, 0);

                int data_length = (int)::SendMessage(hCurScintilla, SCI_GETLENGTH , 0, 0);
                if (data_length <= 0) {
                    throwInfo(file_empty);
                }

                byte* pData = (byte*)::SendMessage(hCurScintilla, SCI_GETCHARACTERPOINTER , 0, 0);
                if (!pData) {
                    throwError(no_scintilla_pointer);
                }

                nppcrypt::InitData         initdata;
                CryptHeaderWriter       header(crypt.hmac);
                std::basic_string<byte> buffer;
                bool                    autoencrypt = false;
                crypt_file_cursave = NppCryptFileSave::DEFAULT;

                if(old_encryption_available) {
                    if(preferences.files.askonsave) {
                        std::wstring asksave_msg;
                        if (filename.size() > 32) {
                            asksave_msg = TEXT("change encryption of ") + filename.substr(0,32) + TEXT("...?");
                        } else {
                            asksave_msg = TEXT("change encryption of ") + filename + TEXT("?");
                        }
                        if (::MessageBox(nppData._nppHandle, asksave_msg.c_str(), TEXT("nppcrypt"), MB_YESNO | MB_ICONQUESTION) != IDYES) {
                            autoencrypt = true;
                        }
                    } else {
                        autoencrypt = true;
                    }
                }
                if(!autoencrypt) {
                    int encoding = (int)::SendMessage(nppData._nppHandle, NPPM_GETBUFFERENCODING, notifyCode->nmhdr.idFrom, 0);
                    bool no_ascii = (encoding != uni8Bit && encoding != uniUTF8 && encoding != uniCookie) ? true : false;

                    if(!dlg_crypt.encryptDialog(&crypt, &initdata.iv, &filename)) {
                        if (old_encryption_available) {
                            crypt = fiter->second;
                            crypt_file_cursave = NppCryptFileSave::OLD_ENCRYPTION;
                        } else {
                            crypt_file_cursave = NppCryptFileSave::NO_ENCRYPTION;
                            return;
                        }
                    }
                }

                nppcrypt::encrypt(pData, data_length, buffer, crypt.options, crypt.password, initdata);
                header.create(crypt.options, initdata, &buffer[0], buffer.size());
                if (!old_encryption_available) {
                    crypt_files.insert(std::pair<std::wstring, CryptInfo>(path, crypt));
                }

                ::SendMessage(hCurScintilla, SCI_BEGINUNDOACTION, 0, 0);
                ::SendMessage(hCurScintilla, SCI_CLEARALL, 0, 0);
                ::SendMessage(hCurScintilla, SCI_APPENDTEXT, header.size(), (LPARAM)header.c_str());
                ::SendMessage(hCurScintilla, SCI_APPENDTEXT, buffer.size(), (LPARAM)&buffer[0]);
                ::SendMessage(hCurScintilla, SCI_ENDUNDOACTION, 0, 0);
            }
        } catch (ExcInfo& exc) {
            if (exc.getID() != ExcInfo::ID::file_empty) {
                msgbox::info(nppData._nppHandle, exc.what(), exc.getURL(), exc.getURLCaption());
            }
        } catch(std::exception& exc) {
            msgbox::error(nppData._nppHandle, exc.what());
        } catch(...) {
            msgbox::error(nppData._nppHandle, "unknown exception!");
        }
        break;
    }
    case NPPN_FILEBEFORECLOSE:
    {
        std::wstring path, filename, extension;
        help::buffer::getPath(notifyCode->nmhdr.idFrom, path, filename, extension);
        if (preferences.files.extension.compare(extension) == 0) {
            crypt_files.erase(path);
        }
        break;
    }
    }
}

// ====================================================================================================================================================================

void EncryptDlg()
{
    try {
        const byte* pData;
        size_t      data_length;
        size_t      sel_start;

        if (!help::scintilla::getSelection(&pData, &data_length, &sel_start)) {
            throwInfo(no_text_selected);
        }

        nppcrypt::InitData             initdata;

        if(dlg_crypt.encryptDialog(&current.crypt, &initdata.iv)) {

            std::basic_string<byte>     buffer;
            nppcrypt::encrypt(pData, data_length, buffer, current.crypt.options, current.crypt.password, initdata);

            CryptHeaderWriter header(current.crypt.hmac);
            header.create(current.crypt.options, initdata, &buffer[0], buffer.size());

            HWND hCurScintilla = help::scintilla::getCurrent();
            ::SendMessage(hCurScintilla, SCI_BEGINUNDOACTION, 0, 0);
            ::SendMessage(hCurScintilla, SCI_TARGETFROMSELECTION, 0, 0);
            ::SendMessage(hCurScintilla, SCI_REPLACETARGET, header.size(), (LPARAM)header.c_str());
            ::SendMessage(hCurScintilla, SCI_SETSEL, sel_start + header.size(), sel_start + header.size());
            ::SendMessage(hCurScintilla, SCI_TARGETFROMSELECTION, 0, 0);
            ::SendMessage(hCurScintilla, SCI_REPLACETARGET, buffer.size(), (LPARAM)&buffer[0]);
            ::SendMessage(hCurScintilla, SCI_SETSEL, sel_start, sel_start + header.size() + buffer.size());
            ::SendMessage(hCurScintilla, SCI_ENDUNDOACTION, 0, 0);

            current.crypt.password.clear();
        }
    } catch (ExcInfo& exc) {
        msgbox::info(nppData._nppHandle, exc.what(), exc.getURL(), exc.getURLCaption());
    } catch (std::exception& exc) {
        msgbox::error(nppData._nppHandle, exc.what());
    } catch (...) {
        msgbox::error(nppData._nppHandle, "unknown exception!");
    }
}

void DecryptDlg()
{
    try
    {
        const byte* pData;
        size_t      data_length;
        size_t      sel_start;

        if (!help::scintilla::getSelection(&pData, &data_length, &sel_start)) {
            throwInfo(no_text_selected);
        }

        nppcrypt::InitData     initdata;
        CryptHeaderReader   header(current.crypt.hmac);

        /* parse header and check hmac if present */
        bool header_found = header.parse(current.crypt.options, initdata, pData, data_length);
        if (header_found) {
            if (current.crypt.hmac.enable) {
                if (current.crypt.hmac.keypreset_id < 0) {
                    if (!dlg_auth.doDialog()) {
                        return;
                    }
                    current.crypt.hmac.hash.key.set(dlg_auth.getInput());
                    if (!header.checkHMAC()) {
                        throwInfo(hmac_auth_failed);
                    }
                } else {
                    if (current.crypt.hmac.keypreset_id >= preferences.getKeyNum()) {
                        throwInvalid(invalid_keypreset_id);
                    } else {
                        current.crypt.hmac.hash.key.set(preferences.getKey(current.crypt.hmac.keypreset_id), 16);
                        if (!header.checkHMAC()) {
                            throwInfo(hmac_auth_failed);
                        }
                    }
                }
            }
        }

        if (dlg_crypt.decryptDialog(&current.crypt, &initdata.iv, NULL, !header_found)) {

            /* check if salt or tag data is mising */
            size_t need_salt_len = (current.crypt.options.key.salt_bytes > 0 && initdata.salt.size() != current.crypt.options.key.salt_bytes) ? current.crypt.options.key.salt_bytes : 0;
            size_t need_tag_len = 0;
            if (nppcrypt::help::checkProperty(current.crypt.options.cipher, nppcrypt::BLOCK)) {
                if (current.crypt.options.mode == nppcrypt::Mode::gcm && initdata.tag.size() != nppcrypt::Constants::gcm_tag_size) {
                    need_tag_len = nppcrypt::Constants::gcm_tag_size;
                } else if (current.crypt.options.mode == nppcrypt::Mode::ccm && initdata.tag.size() != nppcrypt::Constants::ccm_tag_size) {
                    need_tag_len = nppcrypt::Constants::ccm_tag_size;
                } else if (current.crypt.options.mode == nppcrypt::Mode::eax && initdata.tag.size() != nppcrypt::Constants::eax_tag_size) {
                    need_tag_len = nppcrypt::Constants::eax_tag_size;
                }
            }
            if (need_salt_len > 0 || need_tag_len > 0) {
                if (!dlg_initdata.doDialog(&initdata, need_salt_len, need_tag_len)) {
                    return;
                }
            }

            std::basic_string<byte> buffer;
            decrypt(header.getEncrypted(), header.getEncryptedLength(), buffer, current.crypt.options, current.crypt.password, initdata);
            help::scintilla::replaceSelection(buffer);

            current.crypt.password.clear();
        }
    } catch (ExcInfo& exc) {
        msgbox::info(nppData._nppHandle, exc.what(), exc.getURL(), exc.getURLCaption());
    } catch (std::exception& exc) {
        msgbox::error(nppData._nppHandle, exc.what());
    } catch (...) {
        msgbox::error(nppData._nppHandle, "unknown exception!");
    }
}

void HashDlg()
{
    try {
        dlg_hash.doDialog();
    } catch (ExcInfo& exc) {
        msgbox::info(nppData._nppHandle, exc.what(), exc.getURL(), exc.getURLCaption());
    } catch (std::exception& exc) {
        msgbox::error(nppData._nppHandle, exc.what());
    } catch (...) {
        msgbox::error(nppData._nppHandle, "unknown exception!");
    }
}

void RandomDlg()
{
    try {
        dlg_random.doDialog();
    } catch (ExcInfo& exc) {
        msgbox::info(nppData._nppHandle, exc.what(), exc.getURL(), exc.getURLCaption());
    } catch (std::exception& exc) {
        msgbox::error(nppData._nppHandle, exc.what());
    } catch (...) {
        msgbox::error(nppData._nppHandle, "unknown exception!");
    }
}

void ConvertDlg()
{
    try {
        if (help::scintilla::getSelectionLength()) {
            dlg_convert.doDialog();
        } else {
            throwInfo(no_convert_text_selected);
        }
    } catch (ExcInfo& exc) {
        msgbox::info(nppData._nppHandle, exc.what(), exc.getURL(), exc.getURLCaption());
    } catch (std::exception& exc) {
        msgbox::error(nppData._nppHandle, exc.what());
    } catch (...) {
        msgbox::error(nppData._nppHandle, "unknown exception!");
    }
}

void PreferencesDlg()
{
    dlg_preferences.doDialog();
}

void AboutDlg()
{
    dlg_about.doDialog();
}
