// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2013-2023 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/uritests.h>

#include <qt/guiutil.h>
#include <qt/walletmodel.h>

#include <QUrl>

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?label=Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja"));
    QVERIFY(rv.label == QString("Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?amount=100&label=Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Example"));

    uri.setUrl(QString("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?message=Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?message=Example Address", &rv));
    QVERIFY(rv.address == QString("ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja"));
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?req-message=Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?amount=1,000&label=Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("riecoin:ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja?amount=1,000.0&label=Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));
}
