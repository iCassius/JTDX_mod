#ifndef JTDX_WEB_FREQUENCY_HPP
#define JTDX_WEB_FREQUENCY_HPP

#include <QString>

#include "Radio.hpp"

class Bands;

// Web 频率输入的无控件纯策略。HTTP 层先解析，再把结果交给生产频段模型复核。
namespace JtdxWebFrequency
{
  using Frequency = Radio::Frequency;

  struct Result
  {
    bool valid {false};
    Frequency frequency_hz {0};
    QString reason;
  };

  // 只接受十进制 ASCII 整 Hz；不接受符号、空白、小数点、指数或溢出。
  Result parse_hz (QString const& input);

  // 复用 JTDX 的 Bands 模型，拒绝零值、控制接口无法表达的值和 OOB 频率。
  Result validate_hz (Frequency frequency_hz, Bands const& bands);

  // 供 HTTP 适配直接调用的组合入口。
  Result parse_and_validate_hz (QString const& input, Bands const& bands);
}

#endif
