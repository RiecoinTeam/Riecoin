// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/signmessage.h>
#include <key.h>
#include <key_io.h>
#include <rpc/protocol.h>
#include <rpc/request.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <univalue.h>

#include <string>

static RPCMethod verifycode()
{
    return RPCMethod{"verifycode",
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
            + HelpExampleCli("generatecode", "\"ric1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrsk4clvn\"") +
            "\nVerify the code\n"
            + HelpExampleCli("verifycode", "\"ric1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrsk4clvn\" \"code\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("verifycode", "\"ric1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrsk4clvn\", \"code\"")
        },
        [&](const RPCMethod& self, const JSONRPCRequest& request) -> UniValue
        {
            const uint64_t timestamp(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            switch (MessageVerify(std::string{self.Arg<std::string_view>("address")},
                                  std::string{self.Arg<std::string_view>("code")},
                                  std::string{std::to_string(timestamp - (timestamp % 60))})) {
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

static RPCMethod verifymessage()
{
    return RPCMethod{"verifymessage",
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
            + HelpExampleCli("signmessage", "\"ric1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrsk4clvn\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"ric1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrsk4clvn\" \"signature\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("verifymessage", "\"ric1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrsk4clvn\", \"signature\", \"my message\"")
        },
        [](const RPCMethod& self, const JSONRPCRequest& request) -> UniValue
        {
            switch (MessageVerify(std::string{self.Arg<std::string_view>("address")},
                                  std::string{self.Arg<std::string_view>("signature")},
                                  std::string{self.Arg<std::string_view>("message")})) {
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

static RPCMethod signmessagewithprivkey()
{
    return RPCMethod{
        "signmessagewithprivkey",
        "Sign a message with the private key of an address\n",
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
            + HelpExampleCli("verifymessage", "\"ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja\" \"signature\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("signmessagewithprivkey", "\"privkey\", \"my message\"")
        },
        [](const RPCMethod& self, const JSONRPCRequest& request) -> UniValue
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
        {"util", &signmessagewithprivkey},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
