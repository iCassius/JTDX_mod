#ifndef JTDX_WEB_DX_HPP
#define JTDX_WEB_DX_HPP

#include <QString>

namespace JtdxWebDx
{
  struct Result
  {
    bool valid {false};
    QString reason;
    QString call;
    QString grid;
  };

  // 纯校验入口：不访问 QWidget，不生成标准消息，也不触碰 QSO/TX 状态。
  Result normalize (QString call, QString grid = {});
}

#endif
