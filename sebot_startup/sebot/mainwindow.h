#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QPushButton>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <QJsonArray>
#include <qfile.h>
#include <QDebug>
#include <QProcess>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    /* 制作滚动条显示多个按键 */
    QWidget* pSubWidget;
    QScrollArea* pScrollArea;
    QVBoxLayout* pSubLayout;
    QPushButton* pBtnGroup[100];

public slots:
    void onButtonClicked();

};

#endif // MAINWINDOW_H
