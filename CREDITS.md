# 🏆 Credits and Acknowledgments

## Original Project Foundation

This COSPAS-SARSAT beacon generator is based on and inspired by:

**[SARSAT Project by loorisr](https://github.com/loorisr/sarsat)**
- Original MATLAB/GNU Radio implementation
- COSPAS-SARSAT signal analysis and generation concepts
- T.001 standard compliance reference
- Initial BCH error correction algorithms

## Adaptations and Enhancements

### Hardware Implementation
- **Platform Migration**: MATLAB → dsPIC33CK embedded C
- **RF Architecture**: Added ADF4351 + ADL5375 + RA07M4047M chain
- **I/Q Modulation**: Optimized for ADL5375 hardware modulator
- **Power Management**: Dual-mode 100mW/5W for training/exercises

### Software Architecture
- **Modular Design**: Separated RF interface from signal processing
- **Real-time Operation**: ISR-based modulation at 6400 Hz
- **ADRASEC Integration**: Specialized for French emergency training
- **Frequency Safety**: 403 MHz to prevent false COSPAS-SARSAT alerts

### Standards Compliance
- **COSPAS-SARSAT T.001**: Full 1st generation beacon compliance
- **Datasheet Validation**: Against DS70005399D, ADF4351, ADL5375
- **BCH Error Correction**: Optimized for embedded implementation
- **GPS Position Encoding**: PDF-1 and PDF-2 format support

## Technical References

### Primary Documentation
- **COSPAS-SARSAT T.001**: Specification for COSPAS-SARSAT 406 MHz Distress Beacons
- **dsPIC33CK64MC105**: Microchip datasheet DS70005399D
- **ADF4351**: Analog Devices PLL synthesizer datasheet
- **ADL5375**: Analog Devices I/Q modulator datasheet

### Development Tools
- **MPLAB X IDE**: v6.25+ with XC-DSC compiler v3.21
- **Claude Code**: AI-assisted development and optimization
- **GitHub**: Version control and collaboration platform

## Community Contributions

### ADRASEC Network
- **Requirements Definition**: Training and exercise needs
- **Testing Feedback**: Real-world usage validation
- **Safety Guidelines**: 403 MHz frequency selection rationale

### Emergency Services Integration
- **SATER Exercises**: Search and rescue training compatibility
- **CROSS Coordination**: Maritime rescue center requirements
- **Decoder Validation**: 406 MHz receiver testing support

## Licensing Acknowledgments

### Original Work
- **loorisr/sarsat**: Original concept and algorithms
- **License**: Respect of original project licensing terms
- **Attribution**: Maintained in derivative work

### This Implementation
- **License**: Creative Commons CC BY-NC-SA 4.0
- **Usage**: Educational and training purposes only
- **Distribution**: Open source with attribution requirements

## Special Thanks

### Technical Community
- **COSPAS-SARSAT**: International satellite system for search and rescue
- **Microchip**: dsPIC33CK development ecosystem
- **Analog Devices**: RF component documentation and support
- **Open Source Community**: Tools and libraries that made this possible

### Educational Mission
- **ADRASEC Volunteers**: Dedication to search and rescue training
- **SATER Instructors**: Emergency response education excellence
- **Radioamateurs**: Technical expertise and frequency coordination

---

## How to Contribute

If you've been inspired by this work or want to contribute:

1. **Fork the Repository**: Create your own development branch
2. **Follow CC BY-NC-SA 4.0**: Maintain educational licensing
3. **Document Changes**: Clear attribution and modification logs
4. **Safety First**: Maintain 403 MHz frequency restriction
5. **Share Knowledge**: Contribute back to ADRASEC community

## Contact and Support

For technical questions or collaboration:
- **Original Concept**: [loorisr/sarsat](https://github.com/loorisr/sarsat)
- **This Implementation**: See project documentation
- **ADRASEC Training**: Contact your local ADRASEC chapter

---

> 🙏 **Acknowledgment**: This project exists thanks to the foundational work of loorisr and the continuous dedication of the ADRASEC community to search and rescue excellence.

> 🎯 **Mission**: Advancing emergency beacon technology through education, training, and open source collaboration.
