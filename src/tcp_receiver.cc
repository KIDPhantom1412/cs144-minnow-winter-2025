#include "tcp_receiver.hh"
#include "debug.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }
  // 报文的合法性校验
  if ( !SYN_ && !message.SYN ) {
    return; // 混沌期(SYN 未到),尚无序号约定:任何段都不能留下任何状态
  }
  // 非 SYN 段落在绝对序号 0 即非法(那是 SYN 的专属槽位),直接忽略:
  //   1) 防止下面的 -1 在 unwrap==0 时无符号下溢;带 FIN 的非法段更会把
  //      last_byte_ 毒化成天文数字,导致流永不关闭、对端无限重传;
  //   2) 必须在 unwrap 后的绝对坐标上判断,不能比 32 位 seqno == ISN:
  //      seqno 每 2^32 回绕一次,流跑满一圈后同一 seqno 是合法段。
  // checkpoint 用"下一个期望的绝对序号",且须与下面 insert 保持一致,坐标才对得上。
  if ( !message.SYN && message.seqno.unwrap( isn_, reassembler_.writer().bytes_pushed() + 1 ) == 0 ) {
    return;
  }

  // 初次收到SYN报文
  if ( !SYN_ && message.SYN ) {
    SYN_ = true;
    isn_ = message.seqno;
  }

  // 两套坐标的换算:seqno 空间里 SYN 占一位(绝对 0),而 Reassembler 没有
  // SYN 的概念,流索引只数 payload 字节(首字节 = 0),因此:
  //   payload 绝对起点 = unwrap(seqno) + (SYN ? 1 : 0)
  //       (SYN 段的 seqno 指向 SYN 本身,payload 在它的下一位)
  //   stream index = payload 绝对起点 - 1   (流里不存在 SYN 槽位)
  // 两步合并就是 - !message.SYN:非 SYN 段减 1,SYN 段减 0。
  // FIN 不在此处记账:只把"段尾即流尾"的线索(is_last_substring)交给
  // Reassembler,何时生效由它按序重组、关流(is_closed)来判定。
  reassembler_.insert( message.seqno.unwrap( isn_, reassembler_.writer().bytes_pushed() + 1 ) - !message.SYN,
                       message.payload,
                       message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  if ( reassembler_.writer().has_error() ) {
    return { nullopt, 0, true };
  }
  uint16_t window_size
    = min( static_cast<uint64_t>( numeric_limits<uint16_t>::max() ), writer().available_capacity() );
  if ( !SYN_ ) {
    return { nullopt, window_size, false };
  }
  return { Wrap32::wrap( SYN_ + reassembler_.writer().bytes_pushed() + reassembler_.writer().is_closed(), isn_ ),
           window_size,
           false };
}
