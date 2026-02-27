#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Unicode字体转换脚本
将TTF字体文件转换为单片机可用的字体文件格式
"""

import os
import sys
import struct
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

try:
    from fontTools.ttLib import TTFont
except ImportError:
    print("警告: 未安装fontTools库，部分功能可能受限")

class UnicodeFontConverter:
    def __init__(self):
        self.file_header = b'UFNT'  # 文件标识
        self.version = 1
        
    def parse_char_set(self, char_set_file, full_unicode=False):
        """解析字符集文件"""
        chars = set("你好，世界！♩♪♫♬§♭♯♮#，。、；‘【】、《》？：“{}|￥……（）·’”")
        for i in range(32, 127):
            chars.add(chr(i))
        
        if full_unicode:
            chars = self.get_full_unicode_chars()
            return sorted(chars)
        
        if not os.path.exists(char_set_file):
            print(f"警告: 字符集文件 {char_set_file} 不存在")
            return chars
            
        with open(char_set_file, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                for char in line:
                    if ord(char) >= 0x4E00:
                        chars.add(char)
                    elif ord(char) >= 0x20 and ord(char) <= 0x7E:
                        chars.add(char)
        
        return sorted(chars)
    
    def get_full_unicode_chars(self):
        """获取完整的Unicode字符集（常用范围）"""
        chars = set()
        
        unicode_ranges = [
            (0x0020, 0x007F, "Basic Latin"),
            (0x00A0, 0x00FF, "Latin-1 Supplement"),
            (0x0100, 0x017F, "Latin Extended-A"),
            (0x0180, 0x024F, "Latin Extended-B"),
            (0x0250, 0x02AF, "IPA Extensions"),
            (0x02B0, 0x02FF, "Spacing Modifier Letters"),
            (0x0300, 0x036F, "Combining Diacritical Marks"),
            (0x0370, 0x03FF, "Greek and Coptic"),
            (0x0400, 0x04FF, "Cyrillic"),
            (0x0500, 0x052F, "Cyrillic Supplement"),
            (0x0530, 0x058F, "Armenian"),
            (0x0590, 0x05FF, "Hebrew"),
            (0x0600, 0x06FF, "Arabic"),
            (0x0700, 0x074F, "Syriac"),
            (0x0750, 0x077F, "Arabic Supplement"),
            (0x0780, 0x07BF, "Thaana"),
            (0x07C0, 0x07FF, "NKo"),
            (0x0900, 0x097F, "Devanagari"),
            (0x0980, 0x09FF, "Bengali"),
            (0x0A00, 0x0A7F, "Gurmukhi"),
            (0x0A80, 0x0AFF, "Gujarati"),
            (0x0B00, 0x0B7F, "Oriya"),
            (0x0B80, 0x0BFF, "Tamil"),
            (0x0C00, 0x0C7F, "Telugu"),
            (0x0C80, 0x0CFF, "Kannada"),
            (0x0D00, 0x0D7F, "Malayalam"),
            (0x0D80, 0x0DFF, "Sinhala"),
            (0x0E00, 0x0E7F, "Thai"),
            (0x0E80, 0x0EFF, "Lao"),
            (0x0F00, 0x0FFF, "Tibetan"),
            (0x1000, 0x109F, "Myanmar"),
            (0x10A0, 0x10FF, "Georgian"),
            (0x1100, 0x11FF, "Hangul Jamo"),
            (0x1200, 0x137F, "Ethiopic"),
            (0x1380, 0x139F, "Ethiopic Supplement"),
            (0x13A0, 0x13FF, "Cherokee"),
            (0x1400, 0x167F, "Unified Canadian Aboriginal Syllabics"),
            (0x1680, 0x169F, "Ogham"),
            (0x16A0, 0x16FF, "Runic"),
            (0x1700, 0x171F, "Tagalog"),
            (0x1720, 0x173F, "Hanunoo"),
            (0x1740, 0x175F, "Buhid"),
            (0x1760, 0x177F, "Tagbanwa"),
            (0x1780, 0x17FF, "Khmer"),
            (0x1800, 0x18AF, "Mongolian"),
            (0x1900, 0x194F, "Limbu"),
            (0x1950, 0x197F, "Tai Le"),
            (0x1980, 0x19DF, "New Tai Lue"),
            (0x19E0, 0x19FF, "Khmer Symbols"),
            (0x1A00, 0x1A1F, "Buginese"),
            (0x1B00, 0x1B7F, "Balinese"),
            (0x1D00, 0x1D7F, "Phonetic Extensions"),
            (0x1D80, 0x1DBF, "Phonetic Extensions Supplement"),
            (0x1DC0, 0x1DFF, "Combining Diacritical Marks Supplement"),
            (0x1E00, 0x1EFF, "Latin Extended Additional"),
            (0x1F00, 0x1FFF, "Greek Extended"),
            (0x2000, 0x206F, "General Punctuation"),
            (0x2070, 0x209F, "Superscripts and Subscripts"),
            (0x20A0, 0x20CF, "Currency Symbols"),
            (0x20D0, 0x20FF, "Combining Diacritical Marks for Symbols"),
            (0x2100, 0x214F, "Letterlike Symbols"),
            (0x2150, 0x218F, "Number Forms"),
            (0x2190, 0x21FF, "Arrows"),
            (0x2200, 0x22FF, "Mathematical Operators"),
            (0x2300, 0x23FF, "Miscellaneous Technical"),
            (0x2400, 0x243F, "Control Pictures"),
            (0x2440, 0x245F, "Optical Character Recognition"),
            (0x2460, 0x24FF, "Enclosed Alphanumerics"),
            (0x2500, 0x257F, "Box Drawing"),
            (0x2580, 0x259F, "Block Elements"),
            (0x25A0, 0x25FF, "Geometric Shapes"),
            (0x2600, 0x26FF, "Miscellaneous Symbols"),
            (0x2700, 0x27BF, "Dingbats"),
            (0x27C0, 0x27EF, "Miscellaneous Mathematical Symbols-A"),
            (0x27F0, 0x27FF, "Supplemental Arrows-A"),
            (0x2800, 0x28FF, "Braille Patterns"),
            (0x2900, 0x297F, "Supplemental Arrows-B"),
            (0x2980, 0x29FF, "Miscellaneous Mathematical Symbols-B"),
            (0x2A00, 0x2AFF, "Supplemental Mathematical Operators"),
            (0x2B00, 0x2BFF, "Miscellaneous Symbols and Arrows"),
            (0x2C00, 0x2C5F, "Glagolitic"),
            (0x2C60, 0x2C7F, "Latin Extended-C"),
            (0x2C80, 0x2CFF, "Coptic"),
            (0x2D00, 0x2D2F, "Georgian Supplement"),
            (0x2D30, 0x2D7F, "Tifinagh"),
            (0x2D80, 0x2DDF, "Ethiopic Extended"),
            (0x2E00, 0x2E7F, "Supplemental Punctuation"),
            (0x2E80, 0x2EFF, "CJK Radicals Supplement"),
            (0x2F00, 0x2FDF, "Kangxi Radicals"),
            (0x2FF0, 0x2FFF, "Ideographic Description Characters"),
            (0x3000, 0x303F, "CJK Symbols and Punctuation"),
            (0x3040, 0x309F, "Hiragana"),
            (0x30A0, 0x30FF, "Katakana"),
            (0x3100, 0x312F, "Bopomofo"),
            (0x3130, 0x318F, "Hangul Compatibility Jamo"),
            (0x3190, 0x319F, "Kanbun"),
            (0x31A0, 0x31BF, "Bopomofo Extended"),
            (0x31C0, 0x31EF, "CJK Strokes"),
            (0x31F0, 0x31FF, "Katakana Phonetic Extensions"),
            (0x3200, 0x32FF, "Enclosed CJK Letters and Months"),
            (0x3300, 0x33FF, "CJK Compatibility"),
            (0x3400, 0x4DBF, "CJK Unified Ideographs Extension A"),
            (0x4DC0, 0x4DFF, "Yijing Hexagram Symbols"),
            (0x4E00, 0x9FFF, "CJK Unified Ideographs"),
            (0xA000, 0xA48F, "Yi Syllables"),
            (0xA490, 0xA4CF, "Yi Radicals"),
            (0xA500, 0xA63F, "Vai"),
            (0xA640, 0xA69F, "Cyrillic Extended-B"),
            (0xA700, 0xA71F, "Modifier Tone Letters"),
            (0xA720, 0xA7FF, "Latin Extended-D"),
            (0xA800, 0xA82F, "Syloti Nagri"),
            (0xA840, 0xA87F, "Phags-pa"),
            (0xA880, 0xA8DF, "Saurashtra"),
            (0xA900, 0xA92F, "Kayah Li"),
            (0xA930, 0xA95F, "Rejang"),
            (0xAA00, 0xAA5F, "Cham"),
            (0xAC00, 0xD7AF, "Hangul Syllables"),
            (0xE000, 0xF8FF, "Private Use Area"),
            (0xF900, 0xFAFF, "CJK Compatibility Ideographs"),
            (0xFB00, 0xFB4F, "Alphabetic Presentation Forms"),
            (0xFB50, 0xFDFF, "Arabic Presentation Forms-A"),
            (0xFE00, 0xFE0F, "Variation Selectors"),
            (0xFE10, 0xFE1F, "Vertical Forms"),
            (0xFE20, 0xFE2F, "Combining Half Marks"),
            (0xFE30, 0xFE4F, "CJK Compatibility Forms"),
            (0xFE50, 0xFE6F, "Small Form Variants"),
            (0xFE70, 0xFEFF, "Arabic Presentation Forms-B"),
            (0xFF00, 0xFFEF, "Halfwidth and Fullwidth Forms"),
            (0xFFF0, 0xFFFF, "Specials"),
            (0x10000, 0x1007F, "Linear B Syllabary"),
            (0x10080, 0x100FF, "Linear B Ideograms"),
            (0x10100, 0x1013F, "Aegean Numbers"),
            (0x10140, 0x1018F, "Ancient Greek Numbers"),
            (0x10190, 0x101CF, "Ancient Symbols"),
            (0x101D0, 0x101FF, "Phaistos Disc"),
            (0x10280, 0x1029F, "Lycian"),
            (0x102A0, 0x102DF, "Carian"),
            (0x10300, 0x1032F, "Old Italic"),
            (0x10330, 0x1034F, "Gothic"),
            (0x10380, 0x1039F, "Ugaritic"),
            (0x103A0, 0x103DF, "Old Persian"),
            (0x10400, 0x1044F, "Deseret"),
            (0x10450, 0x1047F, "Shavian"),
            (0x10480, 0x104AF, "Osmanya"),
            (0x10800, 0x1083F, "Cypriot Syllabary"),
            (0x10900, 0x1091F, "Phoenician"),
            (0x10920, 0x1093F, "Lydian"),
            (0x10A00, 0x10A5F, "Kharoshthi"),
            (0x12000, 0x123FF, "Cuneiform"),
            (0x12400, 0x1247F, "Cuneiform Numbers and Punctuation"),
            (0x1D000, 0x1D0FF, "Byzantine Musical Symbols"),
            (0x1D100, 0x1D1FF, "Musical Symbols"),
            (0x1D200, 0x1D24F, "Ancient Greek Musical Notation"),
            (0x1D300, 0x1D35F, "Tai Xuan Jing Symbols"),
            (0x1D360, 0x1D37F, "Counting Rod Numerals"),
            (0x1D400, 0x1D7FF, "Mathematical Alphanumeric Symbols"),
            (0x20000, 0x2A6DF, "CJK Unified Ideographs Extension B"),
            (0x2A700, 0x2B73F, "CJK Unified Ideographs Extension C"),
            (0x2B740, 0x2B81F, "CJK Unified Ideographs Extension D"),
            (0x2F800, 0x2FA1F, "CJK Compatibility Ideographs Supplement"),
            (0xE0000, 0xE007F, "Tags"),
            (0xE0100, 0xE01EF, "Variation Selectors Supplement"),
        ]
        
        total_chars = 0
        for start, end, name in unicode_ranges:
            range_chars = 0
            for code in range(start, end + 1):
                try:
                    char = chr(code)
                    chars.add(char)
                    range_chars += 1
                except ValueError:
                    pass
            total_chars += range_chars
            if range_chars > 0:
                print(f"  {name}: {range_chars} 字符")
        
        print(f"Unicode字符总数: {total_chars}")
        return sorted(chars)
    
    def parse_offset_config(self, offset_file):
        """解析字符偏移配置文件"""
        offset_map = {}
        
        if not os.path.exists(offset_file):
            return offset_map
            
        with open(offset_file, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                parts = line.split()
                if len(parts) >= 2:
                    char = parts[0]
                    try:
                        offset = int(parts[1])
                        offset_map[char] = offset
                    except ValueError:
                        print(f"警告: 无效的偏移量 {parts[1]} for character {char}")
        
        return offset_map
    
    def get_char_bitmap(self, font, char, font_size, offset_map):
        """获取字符的位图数据"""
        # 创建临时图像
        img = Image.new('1', (font_size * 2, font_size * 2), 0)
        draw = ImageDraw.Draw(img)
        
        # 应用字符偏移
        y_offset = offset_map.get(char, 0)
        
        # 绘制字符
        try:
            draw.text((0, y_offset), char, font=font, fill=1)
        except Exception as e:
            print(f"警告: 无法渲染字符 '{char}': {e}")
            return None, 0, 0
        
        # 获取字符边界框
        bbox = img.getbbox()
        if not bbox:
            return None, 0, 0
        
        # 裁剪到实际大小
        char_img = img.crop(bbox)
        width, height = char_img.size
        
        # 转换为位图数据
        bitmap_data = []
        for y in range(height):
            byte = 0
            bit_count = 0
            for x in range(width):
                pixel = char_img.getpixel((x, y))
                if pixel > 0:
                    byte |= (1 << (7 - bit_count))
                bit_count += 1
                if bit_count == 8:
                    bitmap_data.append(byte)
                    byte = 0
                    bit_count = 0
            
            # 处理剩余位
            if bit_count > 0:
                bitmap_data.append(byte)
        
        return bytes(bitmap_data), width, height
    
    def parse_size_spec(self, size_spec):
        """解析字体大小规格"""
        sizes = []
        
        # 分割多个尺寸规格
        size_parts = size_spec.split(',')
        
        for part in size_parts:
            part = part.strip()
            if not part:
                continue
                
            # 检查是否为宽x高格式
            if 'x' in part:
                try:
                    width, height = map(int, part.split('x'))
                    sizes.append((width, height))
                except ValueError:
                    print(f"警告: 忽略无效的尺寸规格 '{part}'")
            else:
                # 单个数字，作为正方形尺寸
                try:
                    size = int(part)
                    sizes.append((size, size))
                except ValueError:
                    print(f"警告: 忽略无效的尺寸规格 '{part}'")
        
        return sizes
    
    def convert_font(self, font_path, size_spec, char_set_file, output_dir, offset_file=None, full_unicode=False):
        """转换字体文件"""
        if not os.path.exists(font_path):
            print(f"错误: 字体文件 {font_path} 不存在")
            return False
        
        os.makedirs(output_dir, exist_ok=True)
        
        chars = self.parse_char_set(char_set_file, full_unicode)
        if not chars:
            print("错误: 没有找到有效的字符")
            return False
        
        # 解析偏移配置
        offset_map = self.parse_offset_config(offset_file) if offset_file else {}
        
        # 解析尺寸规格
        sizes = self.parse_size_spec(size_spec)
        if not sizes:
            print("错误: 没有有效的尺寸规格")
            return False
        
        print(f"开始转换字体: {font_path}")
        print(f"字符数量: {len(chars)}")
        print(f"转换尺寸: {', '.join([f'{w}x{h}' for w, h in sizes])}")
        
        success_count = 0
        
        for width, height in sizes:
            # 使用宽度作为字体大小（PIL使用单个尺寸参数）
            font_size = width
            
            # 加载字体
            try:
                font = ImageFont.truetype(font_path, font_size)
            except Exception as e:
                print(f"错误: 无法加载字体文件 {font_path} 大小 {font_size}: {e}")
                continue
            
            # 生成输出文件名
            font_name = Path(font_path).stem
            output_file = os.path.join(output_dir, f"{font_name}_{width}x{height}.ufnt")
            
            print(f"\n转换尺寸: {width}x{height}")
            print(f"输出文件: {output_file}")
        
            # 计算默认宽高（使用字符'中'作为参考）
            default_width, default_height = width, height
            if '中' in chars:
                bitmap, w, h = self.get_char_bitmap(font, '中', font_size, offset_map)
                if bitmap:
                    default_width, default_height = w, h
            
            # 创建字体文件
            with open(output_file, 'wb') as f:
                # 写入文件头
                f.write(self.file_header)  # 4字节: UFNT
                f.write(struct.pack('>HH', default_width, default_height))  # 2+2字节: 默认宽高
                
                # 先收集所有成功转换的字符
                char_entries = []
                for char in chars:
                    unicode = ord(char)
                    bitmap_data, char_width, char_height = self.get_char_bitmap(font, char, font_size, offset_map)
                    
                    if not bitmap_data:
                        try:
                            print(f"警告: 跳过无法渲染的字符 '{char}' (U+{unicode:04X})")
                        except UnicodeEncodeError:
                            print(f"警告: 跳过无法渲染的字符 U+{unicode:04X}")
                        continue
                    
                    data_size = len(bitmap_data)
                    char_entries.append((unicode, char_width, char_height, bitmap_data, data_size))
                
                # 字符已经按Unicode码有序排列（由parse_char_set保证）
                print(f"字符已按Unicode码有序排列，共 {len(char_entries)} 个字符")
                
                # 写入实际的字符数量
                actual_char_count = len(char_entries)
                f.write(struct.pack('>I', actual_char_count))  # 4字节: 实际字符数量
                
                # 计算数据偏移量（文件头 + 字符数量 + 字符索引表）
                data_offset = 8 + 4 + actual_char_count * (4 + 2 + 2 + 4 + 4)  # 文件头(8) + 字符数量(4) + 每个字符的索引信息(16)
                
                # 写入字符索引表
                for unicode, char_width, char_height, bitmap_data, data_size in char_entries:
                    # 写入字符索引信息（与单片机端UnicodeCharInfo结构完全匹配）
                    f.write(struct.pack('>I', unicode))  # 4字节: Unicode码
                    f.write(struct.pack('>H', char_width))   # 2字节: 字符宽度
                    f.write(struct.pack('>H', char_height))  # 2字节: 字符高度
                    f.write(struct.pack('>I', data_offset))  # 4字节: 数据偏移
                    f.write(struct.pack('>I', data_size))    # 4字节: 数据大小
                    
                    data_offset += data_size
                
                # 写入字符位图数据
                for unicode, char_width, char_height, bitmap_data, data_size in char_entries:
                    f.write(bitmap_data)
                
                print(f"成功转换字符数量: {len(char_entries)}")
                success_count += 1
        
        print(f"\n字体转换完成! 成功转换 {success_count}/{len(sizes)} 个尺寸")
        return success_count > 0
    
    def create_sample_char_set(self, output_file):
        """创建示例字符集文件"""
        # 常用中文字符
        common_chinese = "的一是在不了有和人这中大为上个国我以要他时来用们生到作地于出就分对成会可主发年动同工也能下过子说产种面而方后多定行学法所民得经十三之进着等部度家电力里如水化高自二理起小物现实加量都两体制机当使点从业本去把性好应开它合还因由其些然前外天政四日那社义事平形相全表间样与关各重新线内数正心反你明看原又么利比或但质气第向道命此变条只没结解问意建月公无系军很情者最立代想已通并提直题党程展五果料象员革位入常文总次品式活设及管特件长求老头基资边流路级少图山统接知较将组见计别她手角期根论运农指几九区强放决西被干做必战先回则任取据处队南给色光门即保治北造百规热领七海口东导器压志世金增争济阶油思术极交受联什认六共权收证改清己美再采转更单风切打白教速花带安场身车例真务具万每目至达走积示议声报斗完类八离华名确才科张信马节话米整空元况今集温传土许步群广石记需段研界拉林律叫且究观越织装影算低持音众书布复容儿须际商非验连断深难近矿千周委素技备半办青省列习响约支般史感劳便团往酸历市克何除消构府称太准精值号率族维划选标写存候毛亲快效斯院查江型眼王按格养易置派层片始却专状育厂京识适属圆包火住调满县局照参红细引听该铁价严"
        
        # ASCII可打印字符
        ascii_chars = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write("# Unicode字体转换字符集\n")
            f.write("# 包含常用中文字符和ASCII可打印字符\n\n")
            f.write("常用中文字符:\n")
            f.write(common_chinese + "\n\n")
            f.write("ASCII可打印字符:\n")
            f.write(ascii_chars + "\n")
        
        print(f"示例字符集已创建: {output_file}")

def main():
    parser = argparse.ArgumentParser(description='Unicode字体转换工具')
    parser.add_argument('font_path', help='TTF字体文件路径')
    parser.add_argument('size_spec', help='字体大小规格，格式示例: 16, 16x16, 12x13,13x13,14x14')
    parser.add_argument('char_set', nargs='?', default='', help='字符集文件路径（使用--full-unicode时可省略）')
    parser.add_argument('output_dir', help='输出目录')
    parser.add_argument('--offset', help='字符偏移配置文件路径')
    parser.add_argument('--create-sample', action='store_true', help='创建示例字符集文件')
    parser.add_argument('--full-unicode', action='store_true', help='遍历完整Unicode字符表（覆盖字符集文件）')
    
    args = parser.parse_args()
    
    converter = UnicodeFontConverter()
    
    if args.create_sample:
        sample_file = os.path.join(args.output_dir, 'sample_chars.txt')
        converter.create_sample_char_set(sample_file)
        return
    
    success = converter.convert_font(
        args.font_path,
        args.size_spec,
        args.char_set,
        args.output_dir,
        args.offset,
        args.full_unicode
    )
    
    if not success:
        sys.exit(1)

if __name__ == '__main__':
    main()
