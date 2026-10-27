"""Bounded SD configuration model. No hardware access."""
import copy
import math
import xml.etree.ElementTree as ET

MODES = ('Clicky', 'Twist', 'Momentum', 'Flywheel')
LEDS = ('Halo', 'Breath', 'Bands', 'Rainbow', 'Solid', 'Off', 'Knob', 'KnobTrail')
FEEDBACK_MODES = ('Follow', 'Damping', 'Assist', 'Free')
FEEDBACK_DEFAULTS = dict(mode='Follow', strength=25, damping=6, assist=20, voltage=.8, detents=40, speed_rpm=60, assist_ms=250, trigger_rpm=8)
FEEDBACK_TAGS = dict(mode='HapticMode', strength='HapticStrength', damping='HapticDamping', assist='HapticAssist', voltage='HapticVoltage', detents='HapticDetents', speed_rpm='HapticSpeedRPM', assist_ms='HapticAssistMs', trigger_rpm='HapticTriggerRPM')
FEEDBACK_PRESETS = {
    'free': dict(label='自由转动 · 无电机力', wheel_mode='Clicky', feedback={**FEEDBACK_DEFAULTS, 'mode':'Free'}),
    'light': dict(label='轻刻度', wheel_mode='Clicky', feedback={**FEEDBACK_DEFAULTS}),
    'medium': dict(label='中刻度', wheel_mode='Clicky', feedback={**FEEDBACK_DEFAULTS, 'strength':50, 'damping':12, 'voltage':1.5}),
    'heavy': dict(label='重刻度', wheel_mode='Clicky', feedback={**FEEDBACK_DEFAULTS, 'strength':80, 'damping':20, 'voltage':2.4}),
    'drag_light': dict(label='轻阻尼 · 平滑阻力', wheel_mode='Clicky', feedback={**FEEDBACK_DEFAULTS, 'mode':'Damping', 'damping':20}),
    'drag_medium': dict(label='中阻尼 · 平滑阻力', wheel_mode='Clicky', feedback={**FEEDBACK_DEFAULTS, 'mode':'Damping', 'damping':45, 'voltage':1.4}),
    'assist_light': dict(label='轻助转 · 短时帮推', wheel_mode='Clicky', feedback={**FEEDBACK_DEFAULTS, 'mode':'Assist', 'assist':20, 'damping':2, 'voltage':.6, 'assist_ms':200, 'speed_rpm':45}),
    'assist_medium': dict(label='中助转 · 短时帮推', wheel_mode='Clicky', feedback={**FEEDBACK_DEFAULTS, 'mode':'Assist', 'assist':40, 'damping':3, 'voltage':.9, 'assist_ms':300}),
}
FLYWHEEL_DEFAULTS = dict(duration_ms=20000, half_life_ms=6000, trigger_rpm=8, max_rpm=120, brake_percent=55, brake_ms=80, capture_ms=40, boost_percent=100, voltage=.8)
FLYWHEEL_RANGES = dict(duration_ms=(3000,30000), half_life_ms=(1000,12000), trigger_rpm=(3,40), max_rpm=(30,480), brake_percent=(30,85), brake_ms=(30,200), capture_ms=(20,120), boost_percent=(100,180))
FLYWHEEL_TAGS = dict(duration_ms='FlywheelDurationMS', half_life_ms='FlywheelHalfLifeMS', trigger_rpm='FlywheelTriggerRPM', max_rpm='FlywheelMaxRPM', brake_percent='FlywheelBrakePercent', brake_ms='FlywheelBrakeMS', capture_ms='FlywheelCaptureMS', boost_percent='FlywheelBoostPercent', voltage='FlywheelVoltage')
FLYWHEEL_PRESETS = {
    'responsive': dict(label='灵敏快拨 · 峰值1.3倍', settings={**FLYWHEEL_DEFAULTS,'duration_ms':15000,'half_life_ms':6000,'trigger_rpm':3,'capture_ms':20,'max_rpm':360,'boost_percent':130,'voltage':.9}),
    'short': dict(label='短续转 · 约束较轻', settings={**FLYWHEEL_DEFAULTS,'duration_ms':8000,'half_life_ms':2500,'voltage':.5}),
    'balanced': dict(label='均衡 · 建议实测起点', settings={**FLYWHEEL_DEFAULTS}),
    'long': dict(label='长续转 · 目标减速更慢', settings={**FLYWHEEL_DEFAULTS,'duration_ms':30000,'half_life_ms':10000,'voltage':.8}),
}
GAINS = dict(Clicky_P=.5, Clicky_I=0, Twist_P=.65, Twist_I=.2, Momentum_P=.3, Momentum_I=0)

def integer(value, low, high):
    if isinstance(value, bool) or not isinstance(value, int) or not low <= value <= high:
        raise ValueError(f'整数必须在 {low}～{high} 之间')
    return value

def label(value):
    if not isinstance(value, str) or not value.strip() or len(value.encode('utf-8')) > 31 or any(ord(c) < 32 for c in value):
        raise ValueError('名称不能为空，且最多 31 个 UTF-8 字节（约 10 个汉字）')
    return value

def auxiliary_defaults():
    return [dict(label='小键'+str(i), actions=[[0,0],[0,0],[0,0]]) for i in (7,8)]

def validate_binding(binding):
    label(binding['label'])
    if len(binding['actions']) != 3: raise ValueError('每个绑定必须包含三个动作槽')
    for action in binding['actions']:
        if len(action) != 2: raise ValueError('动作格式错误')
        integer(action[0],0,2000); integer(action[1],0,255)


def validate(config):
    c = copy.deepcopy(config)
    if not isinstance(c.get('motor_enabled'), bool): raise ValueError('力反馈开关无效')
    profiles = c.get('profiles', [])
    if not 1 <= len(profiles) <= 32: raise ValueError('配置数量需为 1～32')
    gains = c.setdefault('gains', GAINS.copy())
    for k in GAINS:
        v = gains.get(k, GAINS[k])
        if isinstance(v, bool) or not isinstance(v, (int, float)) or not 0 <= v <= 5: raise ValueError('电机参数超出 0～5')
        gains[k] = v
    for p in profiles:
        label(p['name'])
        if p.setdefault('display_scene', 'Dog') not in ('Dog','Person','Text','Off'):
            raise ValueError('屏幕展示模式无效')
        text = p.setdefault('display_text', '')
        if not isinstance(text, str) or len(text.encode('utf-8')) > 31 or any(ord(ch) < 32 for ch in text):
            raise ValueError('屏幕文字最多31个UTF-8字节（约10个汉字），不能包含换行或控制字符')
        f = {**FEEDBACK_DEFAULTS, **p.get('feedback', {})}; p['feedback'] = f
        if f['mode'] not in FEEDBACK_MODES: raise ValueError('力反馈模式无效')
        for key in ('strength','damping','assist'): integer(f[key], 0, 100)
        integer(f['detents'], 12, 120); integer(f['speed_rpm'], 10, 120)
        integer(f['assist_ms'], 100, 600); integer(f['trigger_rpm'], 3, 30)
        if isinstance(f['voltage'],bool) or not isinstance(f['voltage'],(int,float)) or not math.isfinite(f['voltage']) or not 0 <= f['voltage'] <= 3: raise ValueError('力反馈输出上限需为0～3V')
        if f['trigger_rpm'] >= f['speed_rpm']: raise ValueError('助转触发转速必须低于截止转速')
        if f['mode'] != 'Follow' and p['mode'] != 'Clicky': raise ValueError('阻尼、助转和自由手感需搭配刻度滚动模式')
        fly = {**FLYWHEEL_DEFAULTS, **p.get('flywheel', {})}; p['flywheel'] = fly
        for key, (low, high) in FLYWHEEL_RANGES.items(): integer(fly[key], low, high)
        if isinstance(fly['voltage'],bool) or not isinstance(fly['voltage'],(int,float)) or not math.isfinite(fly['voltage']) or not 0 <= fly['voltage'] <= .9: raise ValueError('无极补偿电压需为0～0.9V')
        if fly['trigger_rpm'] >= fly['max_rpm']: raise ValueError('无极触发转速必须低于目标转速上限')
        if p['mode'] not in MODES or p['led'] not in LEDS: raise ValueError('模式无效')
        if p['direction'] not in (-1, 1): raise ValueError('方向必须为 1 或 -1')
        integer(p.setdefault('led_direction', 1), -1, 1)
        if p['led_direction'] == 0: raise ValueError('亮点方向必须为 1 或 -1')
        integer(p.setdefault('led_offset', 0), 0, 19)
        integer(p['wheel_key'], 0, 255); integer(p['brightness'], 0, 255)
        for color in ('primary', 'secondary'):
            if len(p[color]) != 3: raise ValueError('颜色必须为 RGB 三通道')
            for v in p[color]: integer(v, 0, 255)
        if len(p['buttons']) != 6: raise ValueError('每套配置必须有六个按键')
        p.setdefault('aux_mode','legacy')
        if p['aux_mode'] not in ('legacy','custom','all_custom'): raise ValueError('7、8键模式无效')
        integer(p.setdefault('chord_window_ms',100),50,250)
        extras=p.setdefault('extra_buttons',auxiliary_defaults())
        if len(extras)!=2: raise ValueError('必须包含7、8两个小键配置')
        chords=p.setdefault('chords',[])
        if len(chords)>247: raise ValueError('每套最多247个多键组合，加8个单键共255种')
        used=set()
        for chord in chords:
            mask=integer(chord['mask'],1,255)
            if not mask & (mask-1): raise ValueError('组合键至少选择两个设备按键')
            if mask in used: raise ValueError('同一套配置不能重复设置相同组合键')
            used.add(mask)
        for binding in p['buttons']+extras+chords: validate_binding(binding)
    return c

def from_xml(data):
    if isinstance(data, str): data = data.encode('utf-8')
    if len(data) > 65536 or b'<!' in data: raise ValueError('XML 超过 64KB 或包含不支持的声明')
    root = ET.fromstring(data)
    if root.tag != 'Configuration': raise ValueError('不是 Hapticpad 配置')
    settings = root.find('Settings')
    def get(node, key, default): return node.findtext(key, str(default)) if node is not None else str(default)
    c = {'motor_enabled': get(settings, 'MotorEnabled', 0) == '1', 'gains': {}, 'profiles': []}
    for k,v in GAINS.items(): c['gains'][k] = float(get(settings, k, v))
    for p in root.findall('./Profiles/Profile'):
        profile = dict(name=p.attrib['name'], mode=get(p,'WheelMode','Clicky'), wheel_key=int(get(p,'WheelKey',0)), direction=int(get(p,'WheelDirection',1)), led=get(p,'LED_Mode',get(settings,'LED_Mode','Solid')), brightness=int(get(p,'LED_Brightness',get(settings,'LED_Brightness',35))), buttons=[])
        for tag, key, default in [('LED_Primary','primary','0,48,120'),('LED_Secondary','secondary','0,0,0')]:
            profile[key] = [int(x.strip()) for x in get(p,tag,get(settings,tag,default)).split(',')]
        profile['display_scene'] = get(p,'DisplayScene','Dog')
        profile['display_text'] = get(p,'DisplayText','')
        profile['led_direction'] = int(get(p,'LED_Direction',1))
        profile['led_offset'] = int(get(p,'LED_Offset',0))
        profile['feedback'] = {}
        for key,tag in FEEDBACK_TAGS.items():
            value = get(p,tag,FEEDBACK_DEFAULTS[key])
            profile['feedback'][key] = value if key == 'mode' else (float(value) if key == 'voltage' else int(value))
        profile['flywheel'] = {key:(float(get(p,tag,FLYWHEEL_DEFAULTS[key])) if key == 'voltage' else int(get(p,tag,FLYWHEEL_DEFAULTS[key]))) for key,tag in FLYWHEEL_TAGS.items()}
        for b in p.findall('./MacroButtons/MacroButton'):
            profile['buttons'].append({'label':get(b,'Label','未设置'), 'actions':[[int(x.strip()) for x in a.text.split(',')] for a in b.findall('Action')]})
        profile['aux_mode']=get(p,'AuxiliaryMode','legacy')
        profile['chord_window_ms']=int(get(p,'ChordWindow',100))
        profile['extra_buttons']=auxiliary_defaults()
        auxiliary=p.find('AuxiliaryButtons')
        def binding_from_xml(b):
            return {'label':get(b,'Label','未设置'), 'actions':[[int(x.strip()) for x in a.text.split(',')] for a in b.findall('Action')]}
        if auxiliary is not None:
            profile['extra_buttons']=[binding_from_xml(b) for b in auxiliary.findall('MacroButton')]
        profile['chords']=[]
        for b in p.findall('./Chords/Chord'):
            binding=binding_from_xml(b);binding['mask']=int(b.attrib['mask']);profile['chords'].append(binding)
        c['profiles'].append(profile)
    return validate(c)

def to_xml(config):
    c = validate(config); root = ET.Element('Configuration'); settings = ET.SubElement(root,'Settings')
    def tag(node, key, value): ET.SubElement(node,key).text = str(value)
    tag(settings,'MotorEnabled',int(c['motor_enabled']))
    for k,v in c['gains'].items():
        if k in GAINS: tag(settings,k,v)
    profiles = ET.SubElement(root,'Profiles')
    for p in c['profiles']:
        node = ET.SubElement(profiles,'Profile', name=p['name'])
        tag(node,'DisplayScene',p['display_scene'])
        if p['display_text']: tag(node,'DisplayText',p['display_text'])
        for key, field in [('LED_Mode','led'),('LED_Brightness','brightness'),('LED_Direction','led_direction'),('LED_Offset','led_offset'),('WheelMode','mode'),('WheelKey','wheel_key'),('WheelDirection','direction')]: tag(node,key,p[field])
        tag(node,'LED_Primary',','.join(map(str,p['primary']))); tag(node,'LED_Secondary',','.join(map(str,p['secondary'])))
        for key,xml_tag in FEEDBACK_TAGS.items(): tag(node,xml_tag,p['feedback'][key])
        if p['mode'] == 'Flywheel' or p['flywheel'] != FLYWHEEL_DEFAULTS:
            for key,xml_tag in FLYWHEEL_TAGS.items(): tag(node,xml_tag,p['flywheel'][key])
        buttons = ET.SubElement(node,'MacroButtons')
        for b in p['buttons']:
            button = ET.SubElement(buttons,'MacroButton')
            for a in b['actions']: tag(button,'Action',f'{a[0]},{a[1]}')
            tag(button,'Label',b['label'])
        tag(node,'AuxiliaryMode',p['aux_mode']);tag(node,'ChordWindow',p['chord_window_ms'])
        auxiliary=ET.SubElement(node,'AuxiliaryButtons')
        for b in p['extra_buttons']:
            button=ET.SubElement(auxiliary,'MacroButton')
            for a in b['actions']:tag(button,'Action',f'{a[0]},{a[1]}')
            tag(button,'Label',b['label'])
        chords=ET.SubElement(node,'Chords')
        for b in p['chords']:
            chord=ET.SubElement(chords,'Chord',mask=str(b['mask']))
            for a in b['actions']:tag(chord,'Action',f'{a[0]},{a[1]}')
            tag(chord,'Label',b['label'])
    ET.indent(root, space='  ')
    data = ET.tostring(root,encoding='utf-8',xml_declaration=True)
    if len(data) > 65536: raise ValueError('配置超过固件的 64KB 上限')
    return data

def add_sdr_profile(config):
    """Append a CAT-console input profile, preserving every existing setting."""
    c = validate(config)
    if any(p['name'] == 'SDR通用' for p in c['profiles']):
        return c
    p = copy.deepcopy(c['profiles'][0])
    p.update(name='SDR通用', mode='Clicky', wheel_key=18, direction=1,
             primary=[0, 100, 90], secondary=[0, 10, 20],
             feedback=copy.deepcopy(FEEDBACK_PRESETS['medium']['feedback']))
    p['buttons'] = [dict(label=name, actions=[[0,17],[0,18],[0,112+i]])
                    for i,name in enumerate(('步进减','步进加','模式前','模式后','频段前','频段后'))]
    p.update(chords=[],extra_buttons=auxiliary_defaults(),aux_mode='legacy')
    c['profiles'].append(p)
    to_xml(c)  # Enforce the firmware's count and file-size limits before returning.
    return validate(c)

def configuration_templates(base):
    """Ready-to-use Mac/Win packs; changing a pack never powers the motor."""
    basic = validate(base)
    basic['motor_enabled'] = False
    for p in basic['profiles']:
        p['mode'] = 'Clicky'; p['wheel_key'] = 0
        p['feedback'] = FEEDBACK_DEFAULTS.copy()
    collection = copy.deepcopy(basic); collection['profiles'] = []
    names = {'light':'轻刻度','medium':'中刻度','heavy':'重刻度',
             'drag_light':'轻阻尼','drag_medium':'中阻尼',
             'assist_light':'轻助转','assist_medium':'中助转','free':'自由转'}
    for original in basic['profiles']:
        prefix = 'Mac' if original['name'].startswith('Mac') else 'Win'
        for key, name in names.items():
            preset = FEEDBACK_PRESETS[key]; p = copy.deepcopy(original)
            p['name'] = prefix + name; p['mode'] = preset['wheel_mode']
            p['feedback'] = copy.deepcopy(preset['feedback'])
            collection['profiles'].append(p)
    return {
        'basic': {'label':'基础恢复 · Mac / Win（电机关闭）','config':validate(basic)},
        'haptics': {'label':'手感合集 · Mac / Win 共16套','config':validate(collection)},
    }
