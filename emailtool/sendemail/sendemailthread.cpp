﻿#include "sendemailthread.h"
#include "sendemail/smtpmime.h"

#pragma execution_character_set("utf-8")
#define TIMEMS qPrintable(QTime::currentTime().toString("hh:mm:ss zzz"))

QScopedPointer<SendEmailThread> SendEmailThread::self;
SendEmailThread *SendEmailThread::Instance()
{
    if (self.isNull()) {
        static QMutex mutex;
        QMutexLocker locker(&mutex);
        if (self.isNull()) {
            self.reset(new SendEmailThread);
        }
    }

    return self.data();
}

SendEmailThread::SendEmailThread(QObject *parent) : QThread(parent)
{
    stopped = false;
    emialTitle.clear();
    sendEmailAddr.clear();
    sendEmailPwd.clear();
    receiveEmailAddr.clear();
    contents.clear();
    fileNames.clear();
}

SendEmailThread::~SendEmailThread()
{
    this->stop();
    this->wait(1000);
}

void SendEmailThread::run()
{
    while (!stopped) {
        int count = contents.count();
        if (count > 0) {
            mutex.lock();
            QString content = contents.takeFirst();
            QString fileName = fileNames.takeFirst();
            mutex.unlock();

            EmailSendResult *result = new EmailSendResult;

            QStringList list = sendEmailAddr.split("@");
            QString tempSMTP = list.at(1).split(".").at(0);
            int tempPort = 25;

            //QQ邮箱端口号为465,必须启用SSL协议.
            if (tempSMTP.toUpper() == "QQ") {
                tempPort = 465;
            }

            SmtpClient smtp(QString("smtp.%1.com").arg(tempSMTP), tempPort, tempPort == 25 ? SmtpClient::TcpConnection : SmtpClient::SslConnection);
            smtp.setUser(sendEmailAddr);
            smtp.setPassword(sendEmailPwd);

            //构建邮件主题,包含发件人收件人附件等.
            MimeMessage message;
            message.setSender(new EmailAddress(sendEmailAddr));

            //逐个添加收件人
            QStringList receiver = receiveEmailAddr.split(';');
            for (int i = 0; i < receiver.size(); i++) {
                message.addRecipient(new EmailAddress(receiver.at(i)));
            }

            //构建邮件标题
            message.setSubject(emialTitle);

            //构建邮件正文
            MimeHtml text;
            text.setHtml(content);
            message.addPart(&text);

            //构建附件-报警图像
            if (fileName.length() > 0) {
                QStringList attas = fileName.split(";");
                foreach (QString tempAtta, attas) {
                    QFile *file = new QFile(tempAtta);
                    if (file->exists()) {
                        message.addPart(new MimeAttachment(file));
                    }
                }
            }

            if (!smtp.connectToHost()) {
                result->IsSendSuccess = false;
                result->BriefInfo = "邮件服务器连接失败";
            } else {
                if (!smtp.login()) {
                    result->IsSendSuccess = false;
                    result->BriefInfo = "邮件用户登录失败";
                } else {
                    if (!smtp.sendMail(message)) {
                        result->IsSendSuccess = false;
                        result->BriefInfo = "邮件发送失败";
                    } else {
                        result->IsSendSuccess = true;
                        result->BriefInfo = "邮件发送成功";
                    }
                }
            }

            smtp.quit();
            if (!result->BriefInfo.isEmpty()) {
                emit receiveEmailResult(result);
            }else{
                delete result;
                result = nullptr;
            }
            msleep(1000);
        }
        msleep(100);
    }
    stopped = false;
}

void SendEmailThread::stop()
{
    stopped = true;
}

void SendEmailThread::setEmailTitle(const QString &emailTitle)
{
    this->emialTitle = emailTitle;
}

void SendEmailThread::setSendEmailAddr(const QString &sendEmailAddr)
{
    this->sendEmailAddr = sendEmailAddr;
}

void SendEmailThread::setSendEmailPwd(const QString &sendEmailPwd)
{
    this->sendEmailPwd = sendEmailPwd;
}

void SendEmailThread::setReceiveEmailAddr(const QString &receiveEmailAddr)
{
    this->receiveEmailAddr = receiveEmailAddr;
}

void SendEmailThread::append(const QString &content, const QString &fileName)
{
    mutex.lock();
    contents.append(content);
    fileNames.append(fileName);
    mutex.unlock();
}
