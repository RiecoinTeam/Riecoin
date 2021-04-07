// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2013-2020 The Riecoin developers
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
    uri.setUrl(QString("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?label=Riecoin Donate Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c"));
    QVERIFY(rv.label == QString("Riecoin Donate Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?amount=100&label=Riecoin Donate"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Riecoin Donate"));

    uri.setUrl(QString("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?message=Riecoin Donate Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?message=Riecoin Donate Address", &rv));
    QVERIFY(rv.address == QString("RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c"));
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?req-message=Riecoin Donate Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?amount=1,000&label=Riecoin Donate"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("riecoin:RDonateWTJmv22m8JtrAWhGkUFF6PU4H1c?amount=1,000.0&label=Riecoin Donate"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));
}
