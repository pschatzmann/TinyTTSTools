# G2P Custom Dictionary Example

This example demonstrates how to use custom phoneme dictionaries with the TinyTTSTools library for text-to-phoneme conversion. The example shows how to define specialized vocabularies and switch between custom and default dictionaries.

## Overview

The example shows how to:
- Define a custom phoneme dictionary with specialized terms
- Set and use custom dictionaries for G2P conversion
- Reset to the default dictionary
- Compare phoneme outputs between custom and default dictionaries

**Note**: This example focuses on phoneme conversion only and does not include audio synthesis.

## Key Features

### Custom Dictionary Definition
```cpp
const PhonemeEntry CUSTOM_DICTIONARY[] = {
    {"arduino", "AA R D UW IY N OW"},
    {"microcontroller", "M AY K R OW K AH N T R OW L ER"},
    {"sensor", "S EH N S ER"},
    {"temperature", "T EH M P ER AH CH ER"},
    {"voltage", "V OW L T IH JH"}
};
```

### Dictionary Management
```cpp
// Set custom dictionary
g2p.setPhonemeDictionary((PhonemeEntry*)CUSTOM_DICTIONARY,
                         CUSTOM_DICTIONARY_SIZE, PhonemeType::ARPAbet);

// Reset to default dictionary
g2p.resetPhonemeDictionary();
```

### Phoneme Conversion
```cpp
String phonemes = g2p.textToPhonemes("arduino");
```

## Expected Output

```
G2P Custom Dictionary Example
==============================

--- Using Custom Dictionary ---
Current dictionary size: 5

Custom Dictionary Phoneme Conversions:
Word: "arduino" -> Phonemes: AA R D UW IY N OW
Word: "sensor" -> Phonemes: S EH N S ER
Word: "temperature" -> Phonemes: T EH M P ER AH CH ER
Word: "voltage" -> Phonemes: V OW L T IH JH
Word: "microcontroller" -> Phonemes: M AY K R OW K AH N T R OW L ER

--- Resetting to Default Dictionary ---
Default dictionary size: [varies]

Default Dictionary Phoneme Conversions:
Word: "hello" -> Phonemes: HH AH L OW
Word: "world" -> Phonemes: W ER L D
Word: "test" -> Phonemes: T EH S T
Word: "example" -> Phonemes: IH G Z AE M P AH L
```

## Use Cases

### Technical Vocabulary
Perfect for applications requiring specialized pronunciation of:
- Product names (Arduino, Raspberry Pi)
- Technical terms (microcontroller, sensor)
- Domain-specific jargon

### Multi-language Support
- Define phoneme dictionaries for different languages
- Switch between language-specific vocabularies
- Support dialect variations

### Pronunciation Control
- Override default pronunciations for better clarity
- Add phoneme entries for new words not in default dictionary
- Fine-tune pronunciation for specific applications

## Implementation Notes

### Dictionary Requirements
- **Sorted Order**: Dictionary must be alphabetically sorted by word for binary search
- **ARPAbet Format**: Phonemes should use standard ARPAbet notation
- **Memory Efficiency**: Custom dictionaries use less RAM than large default dictionaries

### Performance Considerations
- Custom dictionaries provide faster lookup for small vocabularies
- Binary search requires sorted word order
- Memory usage scales with dictionary size

## Troubleshooting

### Dictionary Not Working
- Ensure dictionary is sorted alphabetically by word
- Verify ARPAbet phoneme format is correct
- Check that dictionary size matches actual array size

### Compilation Issues
- Include proper TinyTTSTools headers
- Ensure PhonemeEntry structure is available
- Verify G2PDictionaryModel is properly initialized

## Next Steps

To extend this example:
1. **Add Audio Synthesis**: Integrate with TinyTTSTools vocoder for speech output
2. **Dynamic Dictionaries**: Load dictionaries from external storage
3. **Multiple Languages**: Support switching between language dictionaries
4. **Pronunciation Editor**: Create tools for editing custom pronunciations
