void initialize() {
  
}

void loop()
{
  if (isCaptureMode()) {
    // capture
  } else if (isUploadMode()){
    // upload
  } else if (isSleepMode()) {
    // sleep
  }
}

boolean isCaptureMode() {
  if ((nextCaptureTime - curTime) < 1000) {
    while (nextCaptureTime < curTime) nextCaptureTime += captureInt;
    // capture
  }
}

boolean isUploadMode() {
  if ((nextUploadTime - curTime) < 1000) {
    while (nextUploadTime < curTime) nextUploadTime += uploadInt;
    // upload
  }
}
