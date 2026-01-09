# Contributing to CamForge

Thank you for your interest in contributing to CamForge! 🎉

## Ways to Contribute

- 🐛 **Report bugs** - Open an issue describing the problem
- 💡 **Suggest features** - We'd love to hear your ideas
- 📝 **Improve documentation** - Help others understand the project
- 🔧 **Submit code** - Fix bugs or add new features

## Development Setup

### Prerequisites

- [PlatformIO](https://platformio.org/install) (CLI or VSCode extension)
- ESP32-S3 development board
- Python 3.8+ (for PyForge transpiler)

### Building

```bash
# Clone the repository
git clone https://github.com/Debyte404/CamForge.git
cd CamForge

# Build firmware
pio run -e esp32s3

# Upload to device
pio run -e esp32s3 -t upload

# Monitor serial output
pio device monitor
```

## Pull Request Process

1. **Fork** the repository
2. **Create a branch** for your feature: `git checkout -b feat/my-feature`
3. **Make your changes** with clear, atomic commits
4. **Test** your changes on actual hardware if possible
5. **Push** to your fork: `git push origin feat/my-feature`
6. **Open a Pull Request** against `main`

### Commit Message Format

We follow [Conventional Commits](https://www.conventionalcommits.org/):

```
type(scope): description

[optional body]
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`

Examples:
- `feat(ota): add rollback support`
- `fix(camera): resolve frame buffer overflow`
- `docs: update wiring diagram`

## Code Style

- Use 4-space indentation
- Follow existing naming conventions (`camelCase` for functions, `UPPER_CASE` for constants)
- Keep functions focused and small
- Add comments for non-obvious logic
- Use `#pragma once` for header guards

## Hardware Testing

When contributing camera or hardware-related changes:

1. Test on ESP32-S3-DevKitC or compatible board
2. Verify with OV2640 camera module
3. Check both SD card and no-SD modes
4. Test OTA updates if modifying OTA code

## Questions?

- Open a [Discussion](https://github.com/Debyte404/CamForge/discussions)
- Check existing [Issues](https://github.com/Debyte404/CamForge/issues)

Thank you for helping make CamForge better! 🚀
