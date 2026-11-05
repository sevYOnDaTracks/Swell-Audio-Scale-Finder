#include "../Source/Finder.cpp"
#include <iostream>

int main(int argc, char** argv) {
    ScopedJuceInitialiser_GUI gui;
    constexpr int whiteKeys = 2741;
    auto found = matches(whiteKeys);
    if (found.size()!=7 || !found.contains("C majeure") || !found.contains("A mineure naturelle")) return 1;
    if (!matches(145).isEmpty() || !matches(0).isEmpty()) return 2;
    if (pitch(String::fromUTF8("Ré♭"))!=1 || pitch("B#")!=0 || pitch("Cb")!=11 || pitch("xyz")!=-1) return 3;
    Processor processor;
    MidiBuffer midi;
    for (int note : {60,62,64,65,67,69,71,72}) midi.addEvent(MidiMessage::noteOn(1,note,(uint8)100),0);
    AudioBuffer<float> audio(2,64);
    processor.processBlock(audio,midi);
    if (processor.mask.load()!=whiteKeys) return 4;
    MemoryBlock state;
    processor.getStateInformation(state);
    processor.mask.store(0);
    processor.setStateInformation(state.getData(),(int)state.getSize());
    if (processor.mask.load()!=whiteKeys) return 5;
    if (argc>1) {
        std::unique_ptr<AudioProcessorEditor> editor(processor.createEditor());
        auto img=editor->createComponentSnapshot(editor->getLocalBounds());
        FileOutputStream out{File(argv[1])};
        if (!PNGImageFormat().writeImageToStream(img,out)) return 6;
    }
    std::cout<<"Exact matching, aliases, MIDI and state passed\n";
}
