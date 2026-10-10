void twistScroll(){
  if(wheelModeChanged){
    targetAngle = encoder.getAngle();
    motor.controller = MotionControlType::angle;
    motor.PID_velocity.I = Twist_I; //0.2;
    Serial.print("Set I = ");
    Serial.println(Twist_I);
    motor.PID_velocity.P = Twist_P; //0.65f;
    Serial.print("Set P = ");
    Serial.println(Twist_P);
    motor.enable();
    wheelModeChanged = false;
  }

  encoderAngle = encoder.getAngle();

  motor.move(targetAngle);

  interval = 50 / abs(encoderAngle - targetAngle);

  if(abs(encoderAngle - targetAngle) > 0.2){
    if(mouseTimer + interval < millis()){
      if (!profileCommitBusy && sdDetected) {
        wheelActionCheck();
        if(encoderAngle > targetAngle){
          usb_mouse.mouseScroll(0, 1, 0);
        } else {
          usb_mouse.mouseScroll(0, -1, 0);
        }
      }
      mouseTimer = millis();
    } else {
      cancelWheelAction();
    }
  }
  if (profileCommitBusy || fabsf(encoderAngle - targetAngle) <= 0.2f) cancelWheelAction();
}
/*void twistScroll(){ //Uncommment for consant movement instead of twist mode
  if(wheelModeChanged){
    //targetAngle = encoder.getAngle();
    motor.controller = MotionControlType::velocity;
    motor.enable();
    wheelModeChanged = false;
  }

  motor.move(1);

}*/

void notchyWheel(){
  if(wheelModeChanged){
    motor.controller = MotionControlType::angle;
    motor.PID_velocity.I = Clicky_I;
    motor.PID_velocity.P = Clicky_P;
    motor.enable();
    wheelModeChanged = false;
  }

  // Core 0 独占电机与菜单角度；菜单不发送滚轮/滚轮修饰键 HID。
  if(profileSelectMenu){
    if (!profileCommitBusy) {
      int direction = menuStep.update(encoder.getAngle());
      if (direction > 0 && activeProfile + 1 < totalProfiles) ++activeProfile;
      if (direction < 0 && activeProfile > 0) --activeProfile;
      target_angle = menuStep.center;
    }
    motor.move(target_angle);
    return;
  }

  // 原版 round(angle/step) 在半格边界会随噪声/回弹反复切换目标，
  // 每次切换都发 HID 滚动，导致顺转时出现反向滚动。
  if (!profileSelectMenu && !wheelStepReady) {
    float current = encoder.getAngle();
    target_angle = roundf(current / angle_step) * angle_step;
    wheelStep.begin(target_angle, angle_step);
    wheelStepReady = true;
  }

  motor.move(target_angle);
  int direction = wheelStep.update(encoder.getAngle());
  if (direction != 0) {
    target_angle = wheelStep.center;
    if (sdDetected && !profileCommitBusy) {
      wheelActionCheck();
      usb_mouse.mouseScroll(0, direction, 0); // 保持先前候选版的电脑滚动方向。
    }
    motor.move(target_angle);
  } else {
    cancelWheelAction();
  }
}

void wheelActionCheck(){
  if (profileSelectMenu || profileCommitBusy) return;
  wheelKeyTimer = millis();
  if(wheelAction != 0 && !wheelKeyPressed){
    int modifier = checkModifiers(wheelAction);
    uint8_t keycode[6] = { 0 };
    if(modifier != 0){
      usb_keyboard.keyboardReport(0, modifier, 0);
    } else {
      keycode[0] = convertKeycode(wheelAction);
      usb_keyboard.keyboardReport(0, 0, keycode);
    }
    wheelKeyPressed = true;
    delay(10); //Slight delay to ensure keyboard report has been sent before mouse move
  }
}

void cancelWheelAction(){
  if(wheelKeyPressed && wheelKeyTimer + 100 < millis()){
    usb_keyboard.keyboardReport(0, 0, 0);
    wheelKeyPressed = false;
  }
}

void freeSpinning(){
  if(wheelModeChanged){
    motor.controller = MotionControlType::velocity;
    motor.PID_velocity.I = Momentum_I; //0;
    motor.PID_velocity.P = Momentum_P; //0.3f;
    wheelModeChanged = false;
  }

  //motor.disable();

  float vel = encoder.getVelocity(); //Read current encoder velocity

  if(!decelerating){
    if(abs(last_velocity - vel) > 0.1){//Detected deceleration
      if(decelDetected){
        if(debounceTimer + 100 < millis()){
          last_velocity = vel;
          if(vel > 0){
            target_velocity = vel + 2; //Set current velocity as target
          } else {
            target_velocity = vel - 2; //Set current velocity as target
          }
          if(abs(target_velocity) > 6){
            motor.enable();
          }
          debounceTimer = millis();
        }
      } else {
        decelDetected = true;
        debounceTimer = millis();
      }
    } else {
      decelDetected = false;
      last_velocity = vel;
    }
  }

  if(motor.enabled){
    if(vel > 0.2){ //Moving in the positive direction
      if(aceltimer + 400 < micros()){
        target_velocity -= 0.02;
        aceltimer = micros();
        decelerating = true;
      }
    } else if(vel < -0.2){ //Moving in the negative direction
      if(aceltimer + 400 < micros()){
        target_velocity += 0.02;
        aceltimer = micros();
        decelerating = true;
      }
    } else {
      motor.disable();

      encoderAngle = encoder.getAngle();
      lastEncoderAngle = encoderAngle;

      decelerating = false;
      debounceTimer = millis();
      last_velocity = vel;
      target_velocity = 0;
    }
    motor.move(target_velocity);
  }
  // 电脑滚动与电机控制在同一核处理，屏幕分页重绘不再重复发送 HID。
  float current = encoder.getAngle();
  if (sdDetected && !profileSelectMenu && !profileCommitBusy &&
      fabsf(current - lastEncoderAngle) > 0.1f) {
    wheelActionCheck();
    usb_mouse.mouseScroll(0, (current - lastEncoderAngle) * 10, 0);
    lastEncoderAngle = current;
  } else {
    cancelWheelAction();
  }
}
