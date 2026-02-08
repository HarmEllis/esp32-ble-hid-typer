/* PIN validation - mirrors firmware auth_validate_pin_format() */

export function validatePin(pin: string): string | null {
  if (pin.length !== 6) {
    return "PIN must be exactly 6 digits";
  }

  if (!/^\d{6}$/.test(pin)) {
    return "PIN must contain only digits";
  }

  if (pin === "000000") {
    return "PIN cannot be 000000";
  }

  if (pin === "123456" || pin === "654321") {
    return "PIN cannot be a sequential number";
  }

  if (/^(.)\1{5}$/.test(pin)) {
    return "PIN cannot be all the same digit";
  }

  return null;
}
