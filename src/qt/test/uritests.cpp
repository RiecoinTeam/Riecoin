// Copyright (c) 2009-present The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
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
    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?label=Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja"));
    QVERIFY(rv.label == QString("Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?amount=100&label=Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Example"));

    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?message=Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?message=Example Address", &rv));
    QVERIFY(rv.address == QString("ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja"));
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?req-message=Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    // Commas in amounts are not allowed.
    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?amount=1,000&label=Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?amount=1,000.0&label=Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    // There are two amount specifications. The last value wins.
    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?amount=100&amount=200&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja"));
    QVERIFY(rv.amount == 20000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    // The first amount value is correct. However, the second amount value is not valid. Hence, the URI is not valid.
    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?amount=100&amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    // Test label containing a question mark ('?').
    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?amount=100&label=?"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("?"));

    // Escape sequences are not supported.
    uri.setUrl(QString("riecoin:ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja?amount=100&label=%3F"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ric1pv3mxn0d5g59n6w6qkxdmavw767wgwqpg499xssqfkjfu5gjt0wjqkffwja"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("%3F"));
}
