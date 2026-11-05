#include <juce_audio_utils/juce_audio_utils.h>
#include "BinaryData.h"
#include <array>
#include <atomic>
using namespace juce;

static const StringArray names {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
struct Scale { const char* name; std::vector<int> intervals; };
static const Scale scales[] {
    {"majeure",{0,2,4,5,7,9,11}}, {"mineure naturelle",{0,2,3,5,7,8,10}},
    {"dorien",{0,2,3,5,7,9,10}}, {"phrygien",{0,1,3,5,7,8,10}},
    {"lydien",{0,2,4,6,7,9,11}}, {"mixolydien",{0,2,4,5,7,9,10}},
    {"locrien",{0,1,3,5,6,8,10}}, {"mineure harmonique",{0,2,3,5,7,8,11}},
    {"mineure mélodique ascendante",{0,2,3,5,7,9,11}},
    {"pentatonique majeure",{0,2,4,7,9}}, {"pentatonique mineure",{0,3,5,7,10}},
    {"blues mineure",{0,3,5,6,7,10}}, {"par tons",{0,2,4,6,8,10}}
};
static StringArray matches(int mask) {
    StringArray result;
    for (auto& scale : scales)
        for (int root=0; root<12; ++root) {
            int candidate=0;
            for (int step : scale.intervals) candidate |= 1 << ((root+step)%12);
            if (candidate==mask) result.add(names[root]+" "+String::fromUTF8(scale.name));
        }
    return result;
}
static int pitch(String token) {
    token=token.trim().toLowerCase().replace(String::fromUTF8("♯"),"#").replace(String::fromUTF8("♭"),"b");
    if (token.isEmpty()) return -1;
    int alteration=0;
    if (token.endsWith("#") || (token.length()>1 && token.endsWith("b"))) {
        alteration=token.endsWith("#") ? 1 : -1;
        token=token.dropLastCharacters(1);
    }
    const StringArray letters {"c","d","e","f","g","a","b"}, solfege {"do",String::fromUTF8("ré"),"mi","fa","sol","la","si"};
    if (token=="re") token=String::fromUTF8("ré");
    int i=letters.indexOf(token);
    if (i<0) i=solfege.indexOf(token);
    constexpr int natural[] {0,2,4,5,7,9,11};
    return i<0 ? -1 : (natural[i]+alteration+12)%12;
}
class Processor final : public AudioProcessor {
public:
    Processor():AudioProcessor(BusesProperties().withInput("Input",AudioChannelSet::stereo(),true).withOutput("Output",AudioChannelSet::stereo(),true)) {}
    std::atomic<int> mask {0};
    bool isBusesLayoutSupported(const BusesLayout& b) const override {
        return b.getMainInputChannelSet()==b.getMainOutputChannelSet() && (b.getMainOutputChannelSet()==AudioChannelSet::mono() || b.getMainOutputChannelSet()==AudioChannelSet::stereo());
    }
    void processBlock(AudioBuffer<float>&, MidiBuffer& midi) override {
        int added=0;
        for (auto event:midi) if (event.numBytes==3 && (event.data[0]&0xf0)==0x90 && event.data[2]>0) added |= 1 << (event.data[1]%12);
        if (added) mask.fetch_or(added);
        midi.clear();
    }
    void getStateInformation(MemoryBlock& b) override {MemoryOutputStream(b,false).writeInt(mask.load());}
    void setStateInformation(const void* p,int n) override {if(n==4) mask.store(MemoryInputStream(p,4,false).readInt() & 4095);}
    AudioProcessorEditor* createEditor() override;
    const String getName() const override {return "Swell Scale Finder";}
    bool hasEditor() const override{return true;}
    bool acceptsMidi() const override{return true;}
    bool producesMidi() const override{return false;}
    bool isMidiEffect() const override{return false;}
    double getTailLengthSeconds() const override{return 0;}
    void prepareToPlay(double,int) override{}
    void releaseResources() override{}
    int getNumPrograms() override{return 1;}
    int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override{}
    const String getProgramName(int) override{return {};}
    void changeProgramName(int,const String&) override{}
};
class Editor final : public AudioProcessorEditor,private Timer {
    Processor& proc;
    std::array<TextButton,12> keys;
    TextEditor input, result;
    TextButton apply{"Appliquer"},clear{"Effacer"};
    Label status;
    int last=-1;
    Colour orange{0xffff903e},ink{0xff18181b};
    void timerCallback() override {
        int m=proc.mask.load();
        if(m==last) return;
        last=m;
        StringArray selected;
        for(int i=0;i<12;++i) {
            keys[i].setToggleState((m&(1<<i))!=0,dontSendNotification);
            if(m&(1<<i)) selected.add(names[i]);
        }
        status.setText(selected.joinIntoString("   "),dontSendNotification);
        auto found=matches(m);
        result.setText(m==0 ? String::fromUTF8("Sélectionne tes notes pour commencer.") : found.isEmpty() ? "Aucune gamme exacte parmi les 13 types du catalogue." : found.joinIntoString("\n"),false);
        result.setCaretPosition(0);
    }
    void submit() {
        int m=0;
        for(auto t:StringArray::fromTokens(input.getText()," ,;\t","")) {
            if(t.isEmpty()) continue;
            int n=pitch(t);
            if(n<0){status.setText("Note inconnue : "+t,dontSendNotification);return;}
            m |= 1<<n;
        }
        proc.mask.store(m);last=-1;timerCallback();
    }
public:
    Editor(Processor& p):AudioProcessorEditor(p),proc(p) {
        for(int i=0;i<12;++i) {
            auto& k=keys[i];addAndMakeVisible(k);k.setButtonText(names[i]);
            bool black=names[i].length()>1;
            k.setColour(TextButton::buttonColourId,black ? Colour(0xff28282c):Colour(0xffeeeef0));
            k.setColour(TextButton::textColourOffId,black ? Colours::white:ink);
            k.setColour(TextButton::buttonOnColourId,orange);
            k.setColour(TextButton::textColourOnId,ink);
            k.onClick=[this,i]{proc.mask.fetch_xor(1<<i);last=-1;timerCallback();};
        }
        for(int i:{1,3,6,8,10}) keys[i].toFront(false);
        for(Component* c:std::initializer_list<Component*>{&input,&apply,&clear,&status,&result}) addAndMakeVisible(c);
        input.setTextToShowWhenEmpty(String::fromUTF8("Ex. Do Ré Mi Fa Sol La Si"),Colours::grey);
        apply.onClick=input.onReturnKey=[this]{submit();};
        clear.onClick=[this]{proc.mask.store(0);input.clear();last=-1;timerCallback();};
        result.setMultiLine(true);result.setReadOnly(true);result.setFont(FontOptions(18));
        result.setColour(TextEditor::backgroundColourId,Colour(0xff202024));
        apply.setColour(TextButton::buttonColourId,orange);apply.setColour(TextButton::textColourOffId,ink);
        setSize(660,583);timerCallback();startTimerHz(20);
    }
    void paint(Graphics& g) override {
        g.fillAll(ink);g.drawImageWithin(ImageCache::getFromMemory(BinaryData::scale_finder_png,BinaryData::scale_finder_pngSize),24,18,40,40,RectanglePlacement::centred);
        g.setColour(Colours::white);g.setFont(FontOptions(23,Font::bold));g.drawText("SWELL",76,12,200,30,Justification::left);
        g.setColour(orange);g.setFont(FontOptions(14,Font::bold));g.drawText("SCALE FINDER",76,39,200,22,Justification::left);
        g.setColour(Colours::white);g.setFont(FontOptions(16));g.drawText("Coche les touches, joue en MIDI ou saisis ta liste de notes.",24,77,612,28,Justification::left);
        g.setColour(orange);g.drawText("GAMMES EXACTES",24,350,300,24,Justification::left);
        const auto footer = getLocalBounds().removeFromBottom(36).reduced(24, 0);
        g.setColour(Colour(0xff39393e));
        g.drawHorizontalLine(footer.getY(), 24, (float) footer.getRight());
        g.setColour(Colour(0xffbbbabe));
        g.setFont(FontOptions(13));
        g.drawText("v" JucePlugin_VersionString, footer, Justification::centredLeft);
        g.drawText("by sevyondatracks", footer, Justification::centredRight);
    }
    void resized() override {
        input.setBounds(24,116,388,38);apply.setBounds(422,116,106,38);clear.setBounds(538,116,98,38);
        int white=0;
        for(int i=0;i<12;++i) {
            if(names[i].length()==1){keys[i].setBounds(24+white*87,180,86,115);++white;}
            else keys[i].setBounds(24+white*87-22,180,44,55);
        }
        status.setBounds(24,301,612,35);result.setBounds(24,382,612,152);
    }
};
AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Processor();}
