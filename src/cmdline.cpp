/*
This file is part of the nppcrypt
(http://www.github.com/jeanpaulrichter/nppcrypt)

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
#include <algorithm>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <array>
#include <vector>
#include <sys/stat.h>
#include <exception>
#include <codecvt>
#include <memory>
#include "cli11/CLI11.hpp"
#include "crypt.h"
#include "crypt_help.h"
#include "cryptheader.h"
#include "exception.h"
#include "cryptopp/base64.h"  
#include "clihelp.h"

enum class Action : unsigned
{
    encrypt, decrypt, hash
};

struct Arguments
{
    std::string action;
    std::string input;
    std::string output;
    std::string hash;
    std::string password;
    std::string cipher;
    std::string mode;
    std::string encoding;
    std::string keyderivation;
    std::string tag;
    std::string iv;
    std::string salt;
    std::string hmac;
    std::string hash_key;
};

struct CLIOptions
{
    CLI::Option* input;
    CLI::Option* output;
    CLI::Option* hash;
    CLI::Option* password;
    CLI::Option* cipher;
    CLI::Option* mode;
    CLI::Option* encoding;
    CLI::Option* keyderivation;
    CLI::Option* tag;
    CLI::Option* iv;
    CLI::Option* salt;
    CLI::Option* hmac;
    CLI::Option* hash_key;
    CLI::Option* action;
    CLI::Option* noheader;
    CLI::Option* silent;
    CLI::Option* nointeraction;
};

Arguments    args;
CLIOptions   opt;

// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------

static const std::array<std::array<unsigned char, 5>, 6> BOMbytes =
{ { /* first byte = length of BOM */
{ 3, 239, 187, 191, 0 },    /* utf8 */
{ 2, 254, 255, 0, 0 },      /* utf16 big-endian */
{ 2, 255, 254, 0, 0 },      /* utf16 little-endian*/
{ 4, 0, 0, 254, 255 },      /* utf32 big-endian */
{ 4, 255, 254, 0, 0 },      /* utf32 little-endian */
{ 0, 0, 0, 0, 0 }           /* none */
} };

class File
{
public:
    File() : bom(BOM::none) {};
    enum class BOM : unsigned { utf8, utf16be, utf16le, utf32be, utf32le, none };
    static bool exists(const std::string& path)
    {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    };

protected:
    BOM bom;
};

class FileReader : public File
{
public:
    FileReader(const std::string& path)
    {
        try {
            fs.open(path, std::ios::in | std::ios::binary);
            if (!fs.is_open()) {
                throw std::exception();
            }
            fs.exceptions(std::ifstream::badbit);

            fs.seekg(0, fs.end);
            data_length = fs.tellg();
            if (!data_length) {
                throw std::exception();
            }
            fs.seekg(0, fs.beg);

            if (data_length > 4) {
                unsigned char temp[4];
                fs.read(reinterpret_cast<char*>(temp), 4);
                for (size_t i = 0; i < BOMbytes.size(); i++) {
                    unsigned char c = 0;
                    while (c < BOMbytes[i][0] + 1 && BOMbytes[i][c + 1] == temp[c]) {
                        c++;
                    }
                    if (c == BOMbytes[i][0]) {
                        fs.seekg(BOMbytes[i][0], fs.beg);
                        bom = (BOM)i;
                        data_length -= BOMbytes[i][0];
                        break;
                    }
                }
            }
        } catch (...) {
            if (fs.is_open()) {
                fs.close();
            }
        }
    };

    ~FileReader()
    {
        if (fs.is_open()) {
            fs.close();
        }
    };

    bool ready()
    {
        return (fs.is_open() && fs.good());
    };

    bool getData(std::basic_string<unsigned char>& buf)
    {
        if (fs.is_open() && data_length) {
            try {
                buf.resize(data_length);
                fs.seekg(BOMbytes[(int)bom][0], fs.beg);
                fs.read(reinterpret_cast<char*>(&buf[0]), data_length);
                return true;
            }
            catch (...) {}
        }
        return false;
    };

    std::ifstream& getStream()
    {
        if (fs.is_open()) {
            fs.seekg(BOMbytes[(int)bom][0], fs.beg);
        }
        return fs;
    };

    BOM getBOM() { return bom; };

private:
    std::ifstream        fs;
    unsigned long long   data_length;
};

class FileWriter : public File
{
public:
    FileWriter(const std::string& path, BOM bom = BOM::none)
    {
        this->bom = bom;
        try {
            fs.open(path, std::ios::out | std::ios::binary);
            fs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
            if (bom != BOM::none) {
                fs.write((const char*)&BOMbytes[static_cast<unsigned>(bom)][1], BOMbytes[static_cast<unsigned>(bom)][0]);
            }
        } catch (...) {
            if (fs.is_open()) {
                fs.close();
            }
        }
    };

    ~FileWriter()
    {
        if (fs.is_open()) {
            fs.close();
        }
    };

    bool write(const unsigned char* data, size_t length, const char* header = 0, size_t header_length = 0)
    {
        if (fs.is_open() && fs.good()) {
            try {
                if (header != NULL && header_length != 0) {
                    fs.write(header, header_length);
                }
                fs.write((const char*)data, length);
                return true;
            }
            catch (...) {
                if (fs.is_open()) {
                    fs.close();
                }
            }
        }
        return false;
    };

    std::ofstream& getStream()
    {
        return fs;
    };

private:
    std::ofstream fs;
};

// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace help
{
    bool cmpchars(const char* s1, size_t len1, const char* s2, size_t len2)
    {
        if (len1 <= len2) {
            return false;
        }
        for (int i = 0; i < len2; i++) {
            if (s1[i] != s2[i])
                return false;
        }
        return true;
    }

    // replaces delimiter in string with zeros and returns the offset of all substrings (pos)
    void splitArgument(std::string& s, std::vector<size_t>& pos, char delimiter)
    {
        pos.clear();
        if (!s.size()) {
            return;
        }
        pos.push_back(0);
        size_t tpos = 0;
        while ((tpos = s.find(delimiter, tpos)) != std::string::npos && tpos + 1 < s.size()) {
            s[tpos] = 0;
            tpos++;
            pos.push_back(tpos);
        }
    }

    bool setUserData(const char* s, size_t len, nppcrypt::UserData& d, nppcrypt::Encoding default_enc)
    {
        if (cmpchars(s, len, "hex:", 4)) {
            d.set(s + 4, len - 4, nppcrypt::Encoding::base16);
        } else if (cmpchars(s, len, "base32:", 7)) {
            d.set(s + 7, len - 7, nppcrypt::Encoding::base32);
        } else if (cmpchars(s, len, "base64:", 7)) {
            d.set(s + 7, len - 7, nppcrypt::Encoding::base64);
        } else if (cmpchars(s, len, "utf8:", 5)) {
            d.set(s, len, nppcrypt::Encoding::ascii);
        } else {
            d.set(s, len, default_enc);
        }
        return (d.size() > 0);
    }

    bool getUserInput(const char* msg, nppcrypt::UserData& data, nppcrypt::Encoding default_enc, size_t trys, bool repeat, bool echo)
    {
        nppcrypt::secure_string input1, input2;
        size_t counter = 0;
        setEcho(echo);
        while (true) {
            std::cout << msg << ": ";
            readLine(input1);
            if (repeat) {
                std::cout << std::endl << "repeat: ";
                readLine(input2);
                std::cout << std::endl;
                if (input1.compare(input2) != 0) {
                    std::cout << "input did not match!" << std::endl;
                    continue;
                }
            }
            if (setUserData(input1.c_str(), input1.size(), data, default_enc)) {
                break;
            } else {
                std::cout << "invalid input (empty)." << std::endl;
            }
            counter++;
            if (counter >= trys) {
                setEcho(true);
                return false;
            }
        }
        setEcho(true);
        return true;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace check
{
    /* -p --password , default encoding: utf8 */
    void password(nppcrypt::UserData& password)
    {
        if (opt.password->count()) {
            help::setUserData(args.password.c_str(), args.password.size(), password, nppcrypt::Encoding::ascii);
            for (size_t i = 0; i < args.password.size(); i++) {
                args.password[i] = 0;
            }
        }
        if (!password.size()) {
            if (*opt.nointeraction) {
                throwInvalid(missing_password);
            }
            if (!help::getUserInput("enter password", password, nppcrypt::Encoding::ascii, 3, true, false)) {
                throwInvalid(missing_password);
            }
        }
    }

    /* -c --cipher , i.e.: -c aria:32:gcm */
    void cipher(nppcrypt::Options::Crypt& options)
    {
        if (opt.cipher->count()) {
            std::vector<size_t> pos;
            help::splitArgument(args.cipher, pos, ':');

            if (!nppcrypt::help::getCipher(args.cipher.c_str(), options.cipher)) {
                throwInvalid(invalid_cipher);
            }
            if (pos.size() > 1) {
                options.key.length = std::atoi(&args.cipher[pos[1]]) / 8;
                if (pos.size() > 2) {
                    if (!nppcrypt::help::getCipherMode(&args.cipher[pos[2]], options.mode)) {
                        throwInvalid(invalid_mode);
                    }
                } else {
                    if (!nppcrypt::help::checkProperty(options.cipher, nppcrypt::STREAM)) {
                        if (nppcrypt::help::checkCipherMode(options.cipher, nppcrypt::Mode::gcm)) {
                            options.mode = nppcrypt::Mode::gcm;
                        } else {
                            options.mode = nppcrypt::Mode::cbc;
                        }
                    }
                }
            }
        }
    }

    /* -e --encoding, i.e. -e base16:unix:96:true [encoding:eol:linelength:uppercase] */
    void encoding(nppcrypt::Options::Crypt& options)
    {
        if (opt.encoding->count()) {
            std::vector<size_t> pos;
            help::splitArgument(args.encoding, pos, ':');
            if (!nppcrypt::help::getEncoding(args.encoding.c_str(), options.encoding.enc)) {
                throwInvalid(invalid_encoding);
            }
            if (pos.size() > 1) {
                if (!nppcrypt::help::getEOL(&args.encoding[pos[1]], options.encoding.eol)) {
                    throwInvalid(invalid_eol);
                }
                if (pos.size() > 2) {
                    options.encoding.linelength = std::atoi(&args.encoding[pos[2]]);
                    options.encoding.linebreaks = (options.encoding.linelength == 0) ? false : true;
                    if (pos.size() > 3) {
                        if (strcmp(&args.encoding[pos[3]], "true") == 0) {
                            options.encoding.uppercase = true;
                        } else if (strcmp(&args.encoding[pos[3]], "false") == 0) {
                            options.encoding.uppercase = false;
                        } else {
                            throwInvalid(invalid_uppercase);
                        }
                    }
                }
            }
        }
    }

    /* -k --key-derivation , i.e.:
            -k scrypt:13:8:3 [scrypt with N=2^13, r=8, p=3]
            -k pbkdf2:sha3:256:1000 [pbkdf2 with sha3-256 and 1000 iterations]
            -k bcrypt:7 [bcrypt with 2^7 iterations]
    */
    void keyderivation(nppcrypt::Options::Crypt& options)
    {
        if (opt.keyderivation->count()) {
            std::vector<size_t> pos;
            help::splitArgument(args.keyderivation, pos, ':');
            if (!nppcrypt::help::getKeyDerivation(args.keyderivation.c_str(), options.key.algorithm)) {
                throwInvalid(invalid_keyderivation);
            }
            switch (options.key.algorithm) {
            case nppcrypt::KeyDerivation::pbkdf2:
            {
                if (pos.size() > 1) {
                    nppcrypt::Hash thash;
                    if (!nppcrypt::help::getHash(&args.keyderivation[pos[1]], thash)) {
                        throwInvalid(invalid_pbkdf2);
                    }
                    options.key.options[0] = static_cast<int>(thash);
                    if (pos.size() > 2) {
                        options.key.options[1] = std::atoi(&args.keyderivation[pos[2]]) / 8;
                        if (pos.size() > 3) {
                            options.key.options[2] = std::atoi(&args.keyderivation[pos[3]]);
                        } else {
                            options.key.options[2] = nppcrypt::Constants::pbkdf2_iter_default;
                        }
                    } else {
                        options.key.options[1] = 0;
                        options.key.options[2] = nppcrypt::Constants::pbkdf2_iter_default;
                    }
                } else {
                    options.key.options[0] = static_cast<int>(nppcrypt::Constants::pbkdf2_default_hash);
                    options.key.options[1] = static_cast<int>(nppcrypt::Constants::pbkdf2_default_hash_digest);
                    options.key.options[2] = nppcrypt::Constants::pbkdf2_iter_default;
                }

                break;
            }
            case nppcrypt::KeyDerivation::bcrypt:
            {
                if (pos.size() > 1) {
                    options.key.options[0] = std::atoi(&args.keyderivation[pos[1]]);
                } else {
                    options.key.options[0] = nppcrypt::Constants::bcrypt_iter_default;
                }
                break;
            }
            case nppcrypt::KeyDerivation::scrypt:
            {
                if (pos.size() > 1) {
                    options.key.options[0] = std::atoi(&args.keyderivation[pos[1]]);
                    if (pos.size() > 2) {
                        options.key.options[1] = std::atoi(&args.keyderivation[pos[2]]);
                        if (pos.size() > 3) {
                            options.key.options[2] = std::atoi(&args.keyderivation[pos[3]]);
                        } else {
                            options.key.options[2] = nppcrypt::Constants::scrypt_p_default;
                        }
                    } else {
                        options.key.options[1] = nppcrypt::Constants::scrypt_r_default;
                    }
                } else {
                    options.key.options[0] = nppcrypt::Constants::scrypt_N_default;
                }
                break;
            }
            }
        }
    }

    /* --hmac , i.e. --hmac sha2:256 */
    void hmac(CryptHeaderWriter::HMAC& hmac)
    {
        if (opt.hmac->count()) {
            std::vector<size_t> pos;
            help::splitArgument(args.hmac, pos, ':');

            if (!nppcrypt::help::getHash(args.hmac.c_str(), hmac.hash.algorithm) || !nppcrypt::help::checkProperty(hmac.hash.algorithm, nppcrypt::HMAC_SUPPORT)) {
                throwInvalid(invalid_hmac_hash);
            }
            if (pos.size() > 1) {
                hmac.hash.digest_length = (size_t)std::atoi(&args.hmac[pos[1]]) / 8;
                if (!nppcrypt::help::checkHashDigest(hmac.hash.algorithm, hmac.hash.digest_length)) {
                    throwInvalid(invalid_hmac_hash);
                }
            }
            hmac.enable = true;
            hmac.hash.use_key = true;
        }
        if (hmac.enable) {
            if (opt.hash_key->count()) {
                help::setUserData(args.hash_key.c_str(), args.hash_key.size(), hmac.hash.key, nppcrypt::Encoding::ascii);
            }
            if (!hmac.hash.key.size()) {
                if (*opt.nointeraction) {
                    throwInvalid(missing_hmac_key);
                }
                if (!help::getUserInput("enter HMAC key", hmac.hash.key, nppcrypt::Encoding::ascii, 2, true, false)) {
                    throwInvalid(missing_hmac_key);
                }
            }
        }
    }

    /* -t --tag , default-encoding: base64 [decryption] */
    void tag(const nppcrypt::Options::Crypt& options, nppcrypt::UserData& tag)
    {
        if (opt.tag->count()) {
            help::setUserData(args.tag.c_str(), args.tag.size(), tag, nppcrypt::Encoding::base64);
        }
        if (!nppcrypt::help::checkProperty(options.cipher, nppcrypt::STREAM) && (options.mode == nppcrypt::Mode::ccm || options.mode == nppcrypt::Mode::gcm || options.mode == nppcrypt::Mode::eax) && !tag.size()) {
            if (*opt.nointeraction) {
                throwInvalid(invalid_tag);
            }
            if (!help::getUserInput("please specify tag", tag, nppcrypt::Encoding::base64, 2, false, true)) {
                throwInvalid(invalid_tag);
            }
        }
    }

    /* -v --iv , i.e.:
            -v random [default]
            -v YXNkZmFzZGZhc2RmYXNkZg== [custom iv, default-encoding: base64]
            -v base16:01A2C1... */
    void iv(nppcrypt::Options::Crypt& options, nppcrypt::UserData& iv, bool decryption)
    {
        if (opt.iv->count()) {
            if (args.iv.size() == 6 && args.iv.compare("random") == 0) {
                options.iv = nppcrypt::IV::random;
            } else if (args.iv.size() == 4 && args.iv.compare("zero") == 0) {
                options.iv = nppcrypt::IV::zero;
            } else if (args.iv.size() == 13 && args.iv.compare("keyderivation") == 0) {
                options.iv = nppcrypt::IV::keyderivation;
            } else {
                options.iv = nppcrypt::IV::custom;
                if (!help::setUserData(args.iv.c_str(), args.iv.size(), iv, nppcrypt::Encoding::base64)) {
                    throwInvalid(missing_iv);
                }
            }
        }
        if (decryption && (options.iv != nppcrypt::IV::zero && options.iv != nppcrypt::IV::keyderivation) && !iv.size()) {
            if (*opt.nointeraction) {
                throwInvalid(missing_iv);
            }
            if (!help::getUserInput("IV data missing. please specify", iv, nppcrypt::Encoding::base64, 2, false, true)) {
                throwInvalid(missing_iv);
            }
        }
    }

    /* -s --salt [decryption] */
    void salt(nppcrypt::Options::Crypt& options, nppcrypt::UserData& salt)
    {
        if (opt.salt->count()) {
            help::setUserData(args.salt.c_str(), args.salt.size(), salt, nppcrypt::Encoding::base64);
            options.key.salt_bytes = salt.size();
        }
        if (options.key.salt_bytes > 0 && !salt.size()) {
            if (*opt.nointeraction) {
                throwInvalid(missing_salt);
            }
            if (!help::getUserInput("salt data missing. please specify", salt, nppcrypt::Encoding::base64, 2, false, true)) {
                throwInvalid(missing_salt);
            }
        }
    }

    /* -s --salt [encryption] */
    void salt(nppcrypt::Options::Crypt& options)
    {
        if (opt.salt->count()) {
            options.key.salt_bytes = std::atoi(args.salt.c_str());
        }
    }

    /* output file */
    void outputfile()
    {
        if (opt.output->count()) {
            std::fstream f(args.output, std::ios::out | std::ios::binary);
            if (!f.is_open()) {
                throwError(failed_to_write_file);
            }
            f.close();
        }
    }

    /* -h --hash */
    void hash(nppcrypt::Options::Hash& options)
    {
        std::vector<size_t> pos;
        help::splitArgument(args.hash, pos, ':');

        if (!nppcrypt::help::getHash(args.hash.c_str(), options.algorithm)) {
            throwInvalid(invalid_hash);
        }
        if (pos.size() > 1) {
            options.digest_length = std::atoi(&args.hash[pos[1]]) / 8;
            if (!nppcrypt::help::checkHashDigest(options.algorithm, options.digest_length)) {
                throwInvalid(invalid_hash);
            }
        } else {
            options.digest_length = 0;
        }

        if (opt.hash_key->count()) {
            if (!help::setUserData(args.hash_key.c_str(), args.hash_key.size(), options.key, nppcrypt::Encoding::ascii)) {
                if (*opt.nointeraction) {
                    throwInvalid(invalid_hash_key);
                }
                if (!help::getUserInput("enter key", options.key, nppcrypt::Encoding::ascii, 2, true, false)) {
                    throwInvalid(invalid_hash_key);
                }
            }
            options.use_key = true;
        }
        if (options.use_key) {
            if (!nppcrypt::help::checkProperty(options.algorithm, nppcrypt::KEY_SUPPORT)) {
                if (nppcrypt::help::checkProperty(options.algorithm, nppcrypt::HMAC_SUPPORT)) {
                    if (!*opt.silent) {
                        std::cout << nppcrypt::help::getString(options.algorithm) << " does not support key input. using HMAC ..." << std::endl;
                    }
                } else {
                    if (!*opt.silent) {
                        std::cout << nppcrypt::help::getString(options.algorithm) << " does not support key input. ignoring --hash-key ..." << std::endl;
                    }
                    options.use_key = false;
                }
            }
        }

        if (nppcrypt::help::checkProperty(options.algorithm, nppcrypt::KEY_REQUIRED) && !options.use_key) {
            throwInvalid(hash_requires_key);
        }
    }
}

namespace print
{
    void options(const nppcrypt::Options::Crypt& options)
    {
        size_t c_keylen = options.key.length;
        size_t c_ivlen, c_blocksize;
        getCipherInfo(options.cipher, options.mode, c_keylen, c_ivlen, c_blocksize);

        std::cout << "options: " << nppcrypt::help::getString(options.cipher) << "-" << c_keylen * 8;
        if (!nppcrypt::help::checkProperty(options.cipher, nppcrypt::STREAM)) {
            std::cout << "-" << nppcrypt::help::getString(options.mode);
        }
        std::cout << ", iv: " << c_ivlen << " bytes (" << nppcrypt::help::getString(options.iv) << "), " << nppcrypt::help::getString(options.key.algorithm);;

        switch (options.key.algorithm) {
        case nppcrypt::KeyDerivation::pbkdf2:
        {
            std::cout << " (" << nppcrypt::help::getString(nppcrypt::Hash(options.key.options[0])) << "-" << options.key.options[1] * 8 << ", " << options.key.options[2] << " iterations)";
            break;
        }
        case nppcrypt::KeyDerivation::bcrypt:
        {
            std::cout << " (2^" << options.key.options[0] << " iterations)";
            break;
        }
        case nppcrypt::KeyDerivation::scrypt:
        {
            std::cout << " (N:2^" << options.key.options[0] << ", r:" << options.key.options[1] << ", p:" << options.key.options[2] << ")";
        }
        }
        std::cout << ", encoding: " << nppcrypt::help::getString(options.encoding.enc) << std::endl;
    }

    void initdata(const nppcrypt::Options::Crypt& options, const nppcrypt::InitData& initdata)
    {
        using namespace nppcrypt;
        secure_string tstr;
        if (options.key.salt_bytes && initdata.salt.size()) {
            initdata.salt.get(tstr, nppcrypt::Encoding::base64);
            std::cout << "Salt: " << tstr << std::endl;;
        }
        if ((options.mode == Mode::gcm || options.mode == Mode::ccm || options.mode == Mode::eax) && initdata.tag.size()) {
            initdata.tag.get(tstr, nppcrypt::Encoding::base64);
            std::cout << "Tag: " << tstr << std::endl;
        }
        if (options.iv == IV::random && initdata.iv.size()) {
            initdata.iv.get(tstr, nppcrypt::Encoding::base64);
            std::cout << "IV: " << tstr << std::endl;
        }
    }

    void outputfile()
    {
        if (opt.output->count()) {
            std::cout << "output file: " << args.output << std::endl;
        }
    }
}

void hash(const std::string& filename)
{
    std::basic_string<nppcrypt::byte>    buffer;
    std::vector<std::string>          digests;
    nppcrypt::Options::Hash              options;
    std::ostringstream                out;
    /* default algorithms */
    static const nppcrypt::Hash    thashes[5] = { nppcrypt::Hash::crc32, nppcrypt::Hash::md5, nppcrypt::Hash::sha1, nppcrypt::Hash::sha2, nppcrypt::Hash::sha3 };
    static const size_t         thashes_digests[5] = { 4, 16, 20, 32, 32 };

    if (opt.encoding->count() && !nppcrypt::help::getEncoding(args.encoding.c_str(), options.encoding)) {
        throwInvalid(invalid_encoding);
    }

    if (opt.hash->count()) {
        check::hash(options);
        nppcrypt::hash(options, buffer, filename);
        digests.push_back(std::string(buffer.begin(), buffer.end()));
        out << nppcrypt::help::getString(options.algorithm) << "-" << options.digest_length * 8 << ": " << (const char*)buffer.c_str() << std::endl;
    } else {
        for (size_t i = 0; i < 5; i++) {
            options.algorithm = thashes[i];
            options.digest_length = thashes_digests[i];
            nppcrypt::hash(options, buffer, filename);
            out << nppcrypt::help::getString(options.algorithm) << ": " << (const char*)buffer.c_str() << std::endl;
        }
    }

    if (opt.output->count()) {
        std::string temp = out.str();
        FileWriter fout(args.output);
        if (!fout.write((const nppcrypt::byte*)temp.c_str(), temp.size())) {
            throwError(failed_to_write_file);
        }
    } else {
        std::cout << out.str();
        if (!*opt.nointeraction) {
            std::string input;
            std::getline(std::cin, input);
            if (input.size()) {
                std::transform(input.begin(), input.end(), input.begin(), ::tolower);
                size_t i;
                for (i = 0; i < digests.size(); i++) {
                    std::transform(digests[i].begin(), digests[i].end(), digests[i].begin(), ::tolower);
                    if (input.compare(digests[i]) == 0) {
                        break;
                    }
                }
                if (i == digests.size()) {
                    std::cout << "no match." << std::endl;
                } else {
                    if (opt.hash->count()) {
                        std::cout << nppcrypt::help::getString(options.algorithm) << " matches." << std::endl;
                    } else {
                        std::cout << nppcrypt::help::getString(thashes[i]) << " matches." << std::endl;
                    }
                }
            }
        }
    }
}

void hash(const nppcrypt::byte* input, size_t input_length)
{
    std::basic_string<nppcrypt::byte>  buffer;
    nppcrypt::Options::Hash            options;
    std::ostringstream              out;

    if (opt.encoding->count() && !nppcrypt::help::getEncoding(args.encoding.c_str(), options.encoding)) {
        throwInvalid(invalid_encoding);
    }

    if (opt.hash->count()) {
        check::hash(options);
    } else {
        options.algorithm = nppcrypt::Hash::md5;
        options.digest_length = 16;
    }
    nppcrypt::hash(options, buffer, { { input, input_length } });
    out << nppcrypt::help::getString(options.algorithm) << "-" << options.digest_length * 8 << ": " << (const char*)buffer.c_str() << std::endl;
    
    if (opt.output->count()) {
        std::string temp = out.str();
        FileWriter fout(args.output);
        if (!fout.write((const nppcrypt::byte*)temp.c_str(), temp.size())) {
            throwError(failed_to_write_file);
        }
    } else {
        std::cout << out.str();
    }
}

void decrypt(const nppcrypt::byte* input, size_t input_length, File::BOM bom)
{
    std::basic_string<nppcrypt::byte>  outputData;
    nppcrypt::Options::Crypt           options;
    nppcrypt::UserData                 password;
    CryptHeader::HMAC               hmac;
    CryptHeaderReader               header(hmac);
    nppcrypt::InitData                   init;

    if (bom != File::BOM::utf8 && bom != File::BOM::none) {
        throwInvalid(cmdline_only_utf8);
    }

    bool verbose = !*opt.silent;
    bool write_to_file = (opt.output->count() > 0);
    bool user_interaction = !*opt.nointeraction;
    bool got_header = header.parse(options, init, input, input_length);

    check::password(password);
    check::cipher(options);
    check::keyderivation(options);
    check::tag(options, init.tag);
    check::iv(options, init.iv, true);
    check::salt(options, init.salt);
    check::outputfile();
    check::hmac(hmac);

    nppcrypt::help::validate(options);

    if (verbose) {
        print::outputfile();
        print::options(options);
        print::initdata(options, init);
    }

    if (got_header) {
        if (hmac.enable) {
            if (hmac.keypreset_id >= 0) {
                std::cout << "hmac authentication skipped (presets not available)." << std::endl;
            } else if (!header.checkHMAC()) {
                throwInfo(hmac_auth_failed);
            }
        }
        nppcrypt::decrypt(header.getEncrypted(), header.getEncryptedLength(), outputData, options, password, init);
    } else {
        nppcrypt::decrypt(input, input_length, outputData, options, password, init);
    }
    if (opt.output->count()) {
        FileWriter fout(args.output, bom);
        if (!fout.write(outputData.c_str(), outputData.size())) {
            throwError(failed_to_write_file);
        }
    } else {
        std::cout << outputData.c_str() << std::endl;
    }
}

void encrypt(const nppcrypt::byte* input, size_t input_length)
{
    std::basic_string<nppcrypt::byte>  outputData;
    nppcrypt::Options::Crypt           options;
    nppcrypt::UserData                 password;
    CryptHeader::HMAC               hmac;
    CryptHeaderWriter               header(hmac);
    nppcrypt::InitData                 init;

    bool verbose = !*opt.silent;
    bool create_header = !*opt.noheader;
    bool write_to_file = (opt.output->count() > 0);

    check::password(password);
    check::cipher(options);
    check::iv(options, init.iv, false);
    check::keyderivation(options);
    check::salt(options);
    check::encoding(options);
    check::hmac(hmac);
    check::outputfile();

    nppcrypt::help::validate(options);

    if (verbose) {
        print::outputfile();
        print::options(options);
    }
    
    nppcrypt::encrypt(input, input_length, outputData, options, password, init);

    if (create_header) {
        header.create(options, init, outputData.c_str(), outputData.size());
    }
    if (write_to_file) {
        FileWriter fout(args.output);
        if (!fout.write(outputData.c_str(), outputData.size(), header.c_str(), header.size())) {
            throwError(failed_to_write_file);
        }
        if (verbose || !create_header) {
            print::initdata(options, init);
        }
    } else {
        if (header.size()) {
            std::cout << header.c_str();
        }
        if (verbose && !create_header) {
            print::initdata(options, init);
        }
        std::cout << outputData.c_str() << std::endl;
    }
}

int main(int argc, char** argv)
{
    setLocale();
    CLI::App app{ "nppcrypt" };

    try {
        Action action;

        // setup CLI11 parser
        opt.action = app.add_option("action", args.action, "(enc|dec|hash)");
        opt.input = app.add_option("input", args.input, "input (file or string)");
        opt.hash = app.add_option("-a,--algorithm", args.hash, "*hash-algorithm*[:Digestlength] i.e.: sha3:512 (adler32|blake2b|blake2s|cmac_aes|crc32|keccak|md2|md4|md5|ripemd|sha1|sha2|sha3|siphash24|siphash48|sm3|tiger|whirlpool)");
        opt.password = app.add_option("-p,--password", args.password, "[(utf8|hex|base32|base64):]*password* , default encoding: utf8");
        opt.output = app.add_option("-o,--output", args.output, "output file");
        opt.cipher = app.add_option("-c,--cipher", args.cipher, "cipher[:keylength[:mode]] i.e. camellia:256:cbc, default: rijndael:256:gcm\nciphers: (threeway|aria|blowfish|btea|camellia|cast128|cast256|chacha20|des|des_ede2|des_ede3|desx|gost|idea|kalyna128|kalyna256|kalyna512|mars|panama|rc2|rc4|rc5|rc6|rijndael|saferk|safersk|salsa20|seal|seed|serpent|shacal2|shark|simon128|skipjack|sm4|sosemanuk|speck128|square|tea|threefish256|threefish512|threefish1024|twofish|wake|xsalsa20|xtea),\nmodes: (ecb|cbc|cbc_cts|cfb|ofb|ctr|eax|ccm|gcm)");
        opt.keyderivation = app.add_option("-k,--key-derivation", args.keyderivation, "key derivation algorithm [default: scrypt]: (pbkdf2|bcrypt|scrypt)[:*option1*[:*option2*[:*option3*]]]");
        opt.encoding = app.add_option("-e,--encoding", args.encoding, "encoding [default:base64]: (ascii|base16|base32|base64)[:(windows|unix)[:*linelength*[:*uppercase(true|false)*]]]");
        opt.tag = app.add_option("-t,--tag", args.tag, "tag-value: [(utf8|hex|base32|base64):]*tagdata* , default-encoding: base64");
        opt.salt = app.add_option("-s,--salt", args.salt, "salt-value: [(utf8|hex|base32|base64):]*saltdata* , default-encoding: base64");
        opt.iv = app.add_option("-v,--iv", args.iv, "IV: (random|keyderivation|zero) OR [(utf8|hex|base32|base64):]*ivdata* , default encoding: base64");
        opt.hmac = app.add_option("--hmac", args.hmac, "create hmac to authenticate header and encrypted data: hash:length i.e. sha3:256");
        opt.hash_key = app.add_option("--hash-key", args.hash_key, "hash-key: [(utf8|hex|base32|base64):]*key* , default-encoding: utf8");
        opt.noheader = app.add_flag("--noheader", "no header output");
        opt.silent = app.add_flag("--silent", "silent mode");
        opt.nointeraction = app.add_flag("--auto", "no user interaction");

        app.parse(argc, argv);

        if (!*opt.input) {
            // if only one positional argument is present: default to hash
            // ( can probably be done more elegantly ... )
            action = Action::hash;
            args.input.assign(args.action);
        } else {
            if (args.action.compare("hash") == 0) {
                action = Action::hash;
            } else if (args.action.compare("dec") == 0) {
                action = Action::decrypt;
            } else if (args.action.compare("enc") == 0) {
                action = Action::encrypt;
            } else {
                throwInvalid(invalid_cmdline_action);
            }
        }
        
        std::basic_string<nppcrypt::byte> inputData;
        File::BOM bom = File::BOM::none;

        if (File::exists(args.input)) {
            if (action != Action::hash) {
                FileReader fin(args.input);
                bom = fin.getBOM();
                if (!fin.getData(inputData)) {
                    throwError(failed_to_read_file);
                }
            }
            if (!*opt.silent) {
                std::cout << "input (file): " << args.input << std::endl;
            }
        } else {
            inputData.assign(args.input.begin(), args.input.end());
            if (!*opt.silent) {
                std::cout << "input (string): " << args.input << std::endl;
            }
        }

        switch (action) {
        case Action::hash:
        {
            if (File::exists(args.input)) {
                hash(args.input);
            } else {
                hash(inputData.c_str(), inputData.size());
            }
            break;
        }
        case Action::decrypt:
        {
            decrypt(inputData.c_str(), inputData.size(), bom);
            break;
        }
        case Action::encrypt:
        {
            encrypt(inputData.c_str(), inputData.size());
            break;
        }
        }

    } catch (const CLI::Error &e) {
        return app.exit(e);
    } catch (std::exception& e) {
        std::cerr << "error:" << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "unexpected error." << std::endl;
        return 2;
    }
    return 0;
}
