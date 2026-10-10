#pragma once

// 纯逻辑段落步进：菜单以进入角度为原点，Clicky 以邻近格中心为原点。
// 中心移动整整一格，反方向须回拨约 0.6 格才再触发，防止边界抖动。
struct MenuStep {
  float center = 0.0f;
  float step = 0.0f;

  void begin(float angle, float angleStep) {
    center = angle;
    step = angleStep;
  }

  int update(float angle) {
    if (step <= 0.0f) return 0;
    if (angle - center >= step * 0.80f) {
      center += step;
      return 1;
    }
    if (angle - center <= -step * 0.80f) {
      center -= step;
      return -1;
    }
    return 0;
  }
};
