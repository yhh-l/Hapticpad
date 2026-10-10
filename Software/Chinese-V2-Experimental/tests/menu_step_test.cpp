#include "../MacroPad/menu_step.h"
#include <cassert>

int main() {
  MenuStep nav;
  nav.begin(123.45f, 0.15f); // 任意角度进入菜单，不能瞬间跳选项
  assert(nav.update(123.45f) == 0);
  assert(nav.update(123.50f) == 0);
  assert(nav.update(123.45f) == 0); // 编码器噪声不触发
  assert(nav.update(123.59f) == 1);
  assert(nav.update(123.55f) == 0);
  assert(nav.update(123.53f) == 0); // 回弹但尚未跨反向阈值
  assert(nav.update(123.47f) == -1); // 明显反转才退回
  nav.begin(-7.3f, 0.15f);
  assert(nav.update(-7.43f) == -1);
  assert(nav.update(-7.74f) == -1); // 一次调用最多一个事件
  assert(nav.update(-7.74f) == -1); // 后续调用补足高速跨格
  return 0;
}
