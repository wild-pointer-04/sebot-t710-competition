#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //创建一个子窗口，这个子窗口交给滚动区域
    pSubWidget = new QWidget();

    //在窗口（100，100）位置显示150*150大小的滚动区域
    pScrollArea = new QScrollArea(this);
    pScrollArea->setFixedSize(500, 390);
    pScrollArea->setWidgetResizable(true);
    pScrollArea->setWidget(pSubWidget);
    pScrollArea->move(QPoint(0, 10)); // 设置显示区域的位置

    //给子窗口设置一个垂直排版对象，动态添加按钮进去，子窗口会自动调整大小
    pSubLayout = new QVBoxLayout(this);
    pSubWidget->setLayout(pSubLayout);
//    for (auto i = 0; i < 10; ++i)
//    {
//        pBtnGroup[i] = new QPushButton(QString("Btn%1").arg(i), this);		// 设置按键的名字
//        pBtnGroup[i]->setCursor(QCursor(Qt::OpenHandCursor));
//        pBtnGroup[i]->setMinimumSize(QSize(50, 50));
//        connect(pBtnGroup[i], SIGNAL(clicked()), this, SLOT(onButtonClicked())); // 加入槽函数
//        pSubLayout->addWidget(pBtnGroup[i]);
//    }


    //[01] read json
    QFile file("/root/workspace/sebot-t710-competition/sebot_startup/shell/config.json");  // Json文件
    if (!file.open(QIODevice::ReadOnly))
    {
        printf( "读取文件失败！"); // 打开文件失败则直接退出
        return;
    }
    QByteArray data = file.readAll(); // 从文件中读取数据
    file.close(); // 关闭文件

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error == QJsonParseError::NoError) // 如果Json格式正确
    {
        if (document.isObject()) // 如果Json数据是个对象
        {
            QJsonObject object = document.object();
            QStringList keys = object.keys();
            for(int i = 0; i < keys.size(); i++)
            {
                // 获取key-value
                QString key = keys[i];
                QJsonValue value = object.value(key);
                if(value.isString())
                {
                    pBtnGroup[i] = new QPushButton(QString(key), this);		// 设置按键的名字
                    pBtnGroup[i]->setCursor(QCursor(Qt::OpenHandCursor));
                    pBtnGroup[i]->setMinimumSize(QSize(50, 50));
                    pBtnGroup[i]->setToolTip(value.toString());
                    if(key == "All/关闭所有进程")
                    {
                        pBtnGroup[i]->setStyleSheet("background-color:red");
                    }

                    connect(pBtnGroup[i], SIGNAL(clicked()), this, SLOT(onButtonClicked())); // 加入槽函数
                    pSubLayout->addWidget(pBtnGroup[i]);
                }
            }

        }
    }
    else // 如果Json格式不正确
    {
        printf( "Json格式错误");
    }

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onButtonClicked()
{
    QPushButton* btn = (QPushButton*)sender();


    QProcess process;
    QString cmd = btn->toolTip();
    qDebug() << "正在启动: " << btn->text();
    process.start(cmd);
    process.waitForStarted();
    process.waitForFinished();
    qDebug() << "启动成功:" << cmd;
}

