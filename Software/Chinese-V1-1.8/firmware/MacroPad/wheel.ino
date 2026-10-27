void motorTick() {
  static unsigned seenGeneration = ~0u;
  static float center = 0, previousAngle = 0, momentum = 0;
  static float sampledAngle = 0, rawVelocity = 0;
  static uint32_t scrollAt = 0, decayAt = 0, sampledAt = 0;
  static AssistPulse assistPulse;
  static Flywheel flywheel;
  static float flywheelTarget=0;
  encoder.update();
  float raw = encoder.getAngle(); rawTelemetry.store(raw);
  if (configBusy.load()) {
    if (motor.enabled) motor.disable();
    assistPulse.reset();flywheel.reset();flywheelTarget=0;flywheelState.store(0);flywheelRemaining.store(0);flywheelLoadStop.store(false); outputTelemetry.store(0); motorPaused.store(true);
    configAcknowledged.store(true); return;
  }
  configAcknowledged.store(false);
  if (deviceStandby.load()) {
    if (motor.enabled) motor.disable();
    assistPulse.reset();flywheel.reset();flywheelTarget=0;
    center=previousAngle=sampledAngle=raw;rawVelocity=momentum=0;sampledAt=micros();
    flywheelState.store(0);flywheelRemaining.store(0);flywheelArmed.store(false);
    motorPaused.store(true);outputTelemetry.store(0);targetTelemetry.store(0);
    return;
  }
  const FeedbackSettings &s = profile.feedback;
  const float step = 2 * PI / s.detents;
  unsigned currentGeneration = generation.load();
  if (currentGeneration != seenGeneration) {
    center = roundf(raw / step) * step; previousAngle = sampledAngle = raw;
    rawVelocity = momentum = 0; sampledAt = micros(); assistPulse.reset();flywheel.reset();flywheelTarget=0;
    float scale = s.strength / 50.f;
    motor.PID_velocity.P = (profile.mode == 1 ? Twist_P : (profile.mode == 2 || profile.mode == 3) ? Momentum_P : Clicky_P) * scale;
    motor.PID_velocity.I = (profile.mode == 1 ? Twist_I : (profile.mode == 2 || profile.mode == 3) ? Momentum_I : Clicky_I) * scale;
    float cap=profile.mode==3&&!menuOpen.load()?fminf(s.voltage,profile.flywheel.voltage):s.voltage;
    motor.voltage_limit = motor.PID_velocity.limit = cap;
    motor.voltage_sensor_align = s.voltage < 1.5f ? s.voltage : 1.5f;
    motor.velocity_limit = motor.P_angle.limit = (profile.mode==3&&!menuOpen.load()?profile.flywheel.maxRPM:s.speedRPM) * .104719755f;
    motor.LPF_velocity.Tf=profile.mode==3?.006f:.025f;
    motor.PID_velocity.reset(); motor.voltage.q = motor.voltage.d = 0;
    if (profile.mode == 1) center = raw;
    seenGeneration = currentGeneration;
  }
  uint32_t now = micros(), elapsed = now - sampledAt;
  bool freshVelocity=elapsed>=2000;
  bool newRotationReading=false;
  float readingVelocity=rawVelocity;
  if (freshVelocity) {
    newRotationReading=raw!=sampledAngle;
    float dt = elapsed * 1e-6f;
    float velocity = dt < .1f ? (raw - sampledAngle) / dt : 0;
    readingVelocity=velocity;
    rawVelocity += dt / ((profile.mode==3?.006f:.02f) + dt) * (velocity - rawVelocity);
    sampledAngle = raw; sampledAt = now;
  }
  bool inMenu = menuOpen.load();
  uint8_t feedbackMode = inMenu && s.mode != FREE ? uint8_t(FOLLOW) : s.mode;
  bool hasForce = feedbackMode == FOLLOW ? s.strength > 0 : feedbackMode == DAMPING ? s.damping > 0 : feedbackMode == ASSIST && (s.assist > 0 || s.damping > 0);
  bool requested = wantMotor.load() && s.voltage > 0 && hasForce;
  if(profile.mode==3 && !inMenu)requested=requested && profile.flywheel.voltage>0 && feedbackMode==FOLLOW;
  if (requested && !focReady.load()) {
    motorPaused.store(false); motor.enable();
    bool calibrated = motor.initFOC(); focReady.store(calibrated);
    if (!calibrated) wantMotor.store(false);
    sensorSign.store((int)motor.sensor_direction == -1 ? -1 : 1);
    motor.voltage.q = motor.voltage.d = 0; motor.disable();
    generation.fetch_add(1); outputTelemetry.store(0); return;
  }
  int sign = sensorSign.load();
  shaftTelemetry.store(shaftCoordinate(raw, sign, motor.sensor_offset));
  // Scrolling is independent of motor power, including the Free profile.
  bool angleControl = inMenu || (profile.mode != 2 && profile.mode != 3);
  bool flywheelAllowed=profile.mode==3 && !inMenu && requested && focReady.load();
  float flyCap=fminf(s.voltage,profile.flywheel.voltage);
  float driveFraction=motor.enabled&&flyCap>0?fabsf(outputTelemetry.load())/flyCap:0;
  if(freshVelocity || !flywheelAllowed)flywheelTarget=flywheel.update(profile.flywheel,flywheel.active()||!newRotationReading?rawVelocity:readingVelocity,millis(),flywheelAllowed,driveFraction,newRotationReading);
  flywheelSamples.store(flywheel.sampleCount());
  flywheelMeasuredRPM.store(rawVelocity*9.549296586f);flywheelPeakRPM.store(flywheel.peakRPM());flywheelArmed.store(flywheel.armed());
  flywheelLoadStop.store(flywheel.loadStopped());
  flywheelState.store(flywheel.state());flywheelRemaining.store(flywheel.remaining(profile.flywheel,millis()));
  float target = 0;
  if (inMenu || profile.mode == 0) {
    if (fabsf(raw - center) >= step * .65f) {
      int delta = lroundf((raw - center) / step);
      center += delta * step; wheelEvents.fetch_add(delta);
    }
    target = shaftCoordinate(center, sign, motor.sensor_offset);
  } else if (profile.mode == 1) {
    target = shaftCoordinate(center, sign, motor.sensor_offset);
    float delta = raw - center;
    if (fabsf(delta) > .2f && millis() - scrollAt >= 50 / fabsf(delta)) {
      scrollAt = millis(); wheelEvents.fetch_add(delta > 0 ? 1 : -1);
    }
  } else if(profile.mode==3) {
    target=flywheelTarget*sign;
    // HID follows measured angle only: stopped hardware never scrolls itself.
    if(fabsf(raw-previousAngle)>=.1f) {
      int delta=(raw-previousAngle)*10;
      if(delta){previousAngle+=delta*.1f;wheelEvents.fetch_add(delta);}
    }
  } else {
    if (fabsf(rawVelocity) > 6 && fabsf(rawVelocity) > fabsf(momentum)) momentum = rawVelocity;
    if (micros() - decayAt >= 400) {
      decayAt = micros();
      if (fabsf(momentum) > .02f) momentum += momentum > 0 ? -.02f : .02f;
      else momentum = 0;
    }
    if (rawVelocity * momentum < 0) momentum = 0;
    float limit = s.speedRPM * .104719755f;
    momentum = constrain(momentum, -limit, limit);
    target = momentum * sign;
    if (fabsf(raw - previousAngle) >= .1f) {
      int delta = (raw - previousAngle) * 10;
      if (delta) { previousAngle += delta * .1f; wheelEvents.fetch_add(delta); }
    }
  }
  if (!requested) {
    if (motor.enabled) motor.disable();
    assistPulse.reset(); motorPaused.store(true); outputTelemetry.store(0); targetTelemetry.store(target); return;
  }
  // Keep the driver off between gestures, including the unpowered capture.
  // Do not enable/disable it on every idle tick.
  if (profile.mode==3 && !inMenu && !flywheel.active()) {
    if (motor.enabled) motor.disable();
    motor.move(0); // Refresh encoder velocity history during idle/capture, with phases off.
    motor.PID_velocity.reset();motorPaused.store(true);outputTelemetry.store(0);targetTelemetry.store(0);return;
  }
  if(profile.mode==3 && !inMenu) {
    // Zero q with an enabled voltage-mode bridge can still electrically drag
    // a fast hand flick. Open all phases to coast above the decaying target.
    float coastThreshold=fabsf(flywheelTarget)*(motor.enabled?1.02f:.98f);
    if(fabsf(rawVelocity)>=coastThreshold) {
      if(motor.enabled)motor.disable();
      motor.voltage.q=motor.voltage.d=0;
      motor.move(target); // Update SimpleFOC velocity history with driver off.
      motorPaused.store(true);targetTelemetry.store(target);outputTelemetry.store(0);return;
    }
  }
  motorPaused.store(false);
  if (!motor.enabled) motor.enable();
  if (feedbackMode == FOLLOW) {
    assistPulse.reset();
    motor.controller = angleControl ? MotionControlType::angle : MotionControlType::velocity;
    targetTelemetry.store(target);
    if (!angleControl && (profile.mode==3?!flywheel.active():momentum==0)) { motor.disable();motor.PID_velocity.reset();outputTelemetry.store(0);return; }
    motor.move(target);
    // Add passive drag in the same signed shaft coordinate, then cap total Uq.
    if(profile.mode==3 && !inMenu) {
      int direction=target>0?1:-1;
      motor.voltage.q=flywheelDriveVoltage(motor.voltage.q,direction,fminf(s.voltage,profile.flywheel.voltage),flywheel.envelope());
    } else motor.voltage.q = boundedVoltage(motor.voltage.q + dampingVoltage(s, rawVelocity) * sign, s.voltage);
  } else {
    float push = assistPulse.update(s, rawVelocity, millis(), feedbackMode == ASSIST && !inMenu);
    float volts = boundedVoltage(dampingVoltage(s, rawVelocity) + push, s.voltage) * sign;
    motor.controller = MotionControlType::torque; targetTelemetry.store(volts); motor.move(volts);
  }
  if(profile.mode==3 && !inMenu && fabsf(motor.voltage.q)<.001f) {
    motor.disable();motorPaused.store(true);outputTelemetry.store(0);return;
  }
  outputTelemetry.store(motor.voltage.q);
  motor.loopFOC();
}
