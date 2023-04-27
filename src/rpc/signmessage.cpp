// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2013-2023 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <key_io.h>
#include <rpc/protocol.h>
#include <rpc/request.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <univalue.h>
#include <util/message.h>

#include <string>

static RPCHelpMan verifycode()
{
    return RPCHelpMan{"verifycode",
        "Verify a code.",
        {
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The Riecoin address to use for the code."},
            {"code", RPCArg::Type::STR, RPCArg::Optional::NO, "The provided code (see signmessage)."},
        },
        RPCResult{
            RPCResult::Type::BOOL, "", "Whether the code is valid or not. Note that a code expires the next minute."
        },
        RPCExamples{
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the code\n"
            + HelpExampleCli("generatecode", "\"ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja\"") +
            "\nVerify the code\n"
            + HelpExampleCli("verifycode", "\"ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja\" \"code\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("verifycode", "\"ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja\", \"code\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::string strAddress = request.params[0].get_str();
            std::string strSign = request.params[1].get_str();
            const uint64_t timestamp(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            const std::string& timestampStr(std::to_string(timestamp - (timestamp % 60)));

            switch (MessageVerify(strAddress, strSign, timestampStr)) {
            case MessageVerificationResult::ERR_INVALID_ADDRESS:
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
            case MessageVerificationResult::ERR_ADDRESS_NO_KEY:
                throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
            case MessageVerificationResult::ERR_MALFORMED_SIGNATURE:
                throw JSONRPCError(RPC_TYPE_ERROR, "Malformed base64 encoding");
            case MessageVerificationResult::ERR_PUBKEY_NOT_RECOVERED:
            case MessageVerificationResult::ERR_NOT_SIGNED:
            case MessageVerificationResult::INCONCLUSIVE:
            case MessageVerificationResult::ERR_INVALID:
            case MessageVerificationResult::ERR_POF:
                return false;
            case MessageVerificationResult::OK_TIMELOCKED:
            case MessageVerificationResult::OK:
                return true;
            }

            return false;
        },
    };
}

static RPCHelpMan verifymessage()
{
    return RPCHelpMan{"verifymessage",
        "Verify a signed message.",
        {
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The Riecoin address to use for the signature."},
            {"signature", RPCArg::Type::STR, RPCArg::Optional::NO, "The signature provided by the signer in base 64 encoding (see signmessage)."},
            {"message", RPCArg::Type::STR, RPCArg::Optional::NO, "The message that was signed."},
        },
        RPCResult{
            RPCResult::Type::BOOL, "", "If the signature is verified or not."
        },
        RPCExamples{
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja\" \"signature\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("verifymessage", "\"ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja\", \"signature\", \"my message\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::string strAddress = request.params[0].get_str();
            std::string strSign = request.params[1].get_str();
            std::string strMessage = request.params[2].get_str();

            switch (MessageVerify(strAddress, strSign, strMessage)) {
            case MessageVerificationResult::ERR_INVALID_ADDRESS:
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
            case MessageVerificationResult::ERR_ADDRESS_NO_KEY:
                throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
            case MessageVerificationResult::ERR_MALFORMED_SIGNATURE:
                throw JSONRPCError(RPC_TYPE_ERROR, "Malformed base64 encoding");
            case MessageVerificationResult::ERR_POF:
                throw JSONRPCError(RPC_TYPE_ERROR, "BIP-322 Proof of funds is not yet supported"); // TODO: get access to UTXO set / mempool to handle this, then remove this error code?
            case MessageVerificationResult::INCONCLUSIVE:
                return false; // TODO: switch to a string based result? mix bool and strings?
            case MessageVerificationResult::ERR_INVALID:
            case MessageVerificationResult::ERR_PUBKEY_NOT_RECOVERED:
            case MessageVerificationResult::ERR_NOT_SIGNED:
                return false;
            case MessageVerificationResult::OK_TIMELOCKED:
                // TODO: switch to string based result? mix bool and strings?
            case MessageVerificationResult::OK:
                return true;
            }

            return false;
        },
    };
}

static RPCHelpMan generatecode()
{
    return RPCHelpMan{"generatecode",
        "\nGenerate a code with the private key of an address\n",
        {
            {"privkey", RPCArg::Type::STR, RPCArg::Optional::NO, "The private key to generate the message with."},
        },
        RPCResult{
            RPCResult::Type::STR, "code", "The code"
        },
        RPCExamples{
            "\nCreate the code\n"
            + HelpExampleCli("generatecode", "\"privkey\"") +
            "\nVerify the code\n"
            + HelpExampleCli("verifycode", "\"ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja\" \"code\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("generatecode", "\"ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::string strPrivkey = request.params[0].get_str();
            const uint64_t timestamp(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            const std::string& timestampStr(std::to_string(timestamp - (timestamp % 60)));

            CKey key = DecodeSecret(strPrivkey);
            if (!key.IsValid()) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
            }

            std::string signature;

            if (!MessageSign(key, timestampStr, signature)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Code generation failed");
            }

            return signature;
        },
    };
}

static RPCHelpMan signmessagewithprivkey()
{
    return RPCHelpMan{"signmessagewithprivkey",
        "\nSign a message with the private key of an address\n",
        {
            {"privkey", RPCArg::Type::STR, RPCArg::Optional::NO, "The private key to sign the message with."},
            {"message", RPCArg::Type::STR, RPCArg::Optional::NO, "The message to create a signature of."},
        },
        RPCResult{
            RPCResult::Type::STR, "signature", "The signature of the message encoded in base 64"
        },
        RPCExamples{
            "\nCreate the signature\n"
            + HelpExampleCli("signmessagewithprivkey", "\"privkey\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja\" \"signature\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("signmessagewithprivkey", "\"privkey\", \"my message\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::string strPrivkey = request.params[0].get_str();
            std::string strMessage = request.params[1].get_str();

            CKey key = DecodeSecret(strPrivkey);
            if (!key.IsValid()) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
            }

            std::string signature;

            if (!MessageSign(key, strMessage, signature)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");
            }

            return signature;
        },
    };
}

void RegisterSignMessageRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"util", &verifycode},
        {"util", &verifymessage},
        {"util", &generatecode},
        {"util", &signmessagewithprivkey},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
