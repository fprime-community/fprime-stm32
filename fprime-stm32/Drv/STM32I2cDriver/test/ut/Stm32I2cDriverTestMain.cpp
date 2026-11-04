// ======================================================================
// \title  Stm32I2cDriverTestMain.cpp
// \author ivanlara
// \brief  cpp file for Stm32I2cDriver component test main function
// ======================================================================

#include "Stm32I2cDriverTester.hpp"

TEST(Nominal, OpenSuccessFast) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testOpenSuccessFast();
}

TEST(Nominal, OpenSuccessStandard) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testOpenSuccessStandard();
}

TEST(Nominal, OpenSuccessFastPlus) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testOpenSuccessFastPlus();
}

TEST(Nominal, OpenFailure) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testOpenFailure();
}

TEST(Nominal, OpenDefaultsToPolled) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testOpenDefaultsToPolled();
}

TEST(Nominal, OpenDma) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testOpenDma();
}

TEST(Nominal, WriteAsyncSuccess) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteAsyncSuccess();
}

TEST(Nominal, ReadAsyncSuccess) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testReadAsyncSuccess();
}

TEST(Nominal, WriteAsyncFailure) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteAsyncFailure();
}

TEST(Nominal, ReadAsyncFailure) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testReadAsyncFailure();
}

TEST(Nominal, WriteAsyncBusyRejectsSecondRequest) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteAsyncBusyRejectsSecondRequest();
}

TEST(Nominal, WriteAsyncTimeout) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteAsyncTimeout();
}

TEST(Nominal, WriteAsyncBeforeOpen) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteAsyncBeforeOpen();
}

TEST(Nominal, ReadAsyncBeforeOpen) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testReadAsyncBeforeOpen();
}

TEST(Nominal, WriteReadAsyncBeforeOpen) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteReadAsyncBeforeOpen();
}

TEST(Nominal, WriteAsyncRejectedWhenNotDma) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteAsyncRejectedWhenNotDma();
}

TEST(Nominal, WriteReadAsyncSuccess) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteReadAsyncSuccess();
}

TEST(Nominal, WriteReadAsyncWriteFailureShortCircuits) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteReadAsyncWriteFailureShortCircuits();
}

TEST(Nominal, WriteReadAsyncReadPhaseFailure) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteReadAsyncReadPhaseFailure();
}

TEST(Nominal, PollNoOpWhenIdle) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testPollNoOpWhenIdle();
}

TEST(Nominal, WriteBeforeOpen) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteBeforeOpen();
}

TEST(Nominal, ReadBeforeOpen) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testReadBeforeOpen();
}

TEST(Nominal, WriteReadBeforeOpen) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteReadBeforeOpen();
}

TEST(Nominal, WriteSuccess) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteSuccess();
}

TEST(Nominal, WriteFailure) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteFailure();
}

TEST(Nominal, WriteLargeBufferCapped) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteLargeBufferCapped();
}

TEST(Nominal, ReadSuccess) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testReadSuccess();
}

TEST(Nominal, ReadFailure) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testReadFailure();
}

TEST(Nominal, ReadLargeBufferCapped) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testReadLargeBufferCapped();
}

TEST(Nominal, WriteReadSuccess) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteReadSuccess();
}

TEST(Nominal, WriteReadTransmitFailureShortCircuits) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteReadTransmitFailureShortCircuits();
}

TEST(Nominal, WriteReadReceiveFailure) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testWriteReadReceiveFailure();
}

TEST(Nominal, TwoInstancesDoNotInterfere) {
    Stm32::Stm32I2cDriverTester tester;
    tester.testTwoInstancesDoNotInterfere();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
