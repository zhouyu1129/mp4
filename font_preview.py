#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
字体预览脚本
生成字体预览图片，支持中英文混合文本预览
"""

import os
import sys
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

class FontPreviewGenerator:
    def __init__(self):
        self.default_bg_color = (255, 255, 255)  # 白色背景
        self.default_fg_color = (0, 0, 0)        # 黑色前景
        self.margin = 20                          # 边距
        self.line_spacing = 10                    # 行间距
    
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
        
    def generate_single_preview(self, font_path, width, height, preview_text, output_path=None, 
                              bg_color=None, fg_color=None, show_grid=False):
        """生成单个尺寸的字体预览图片"""
        
        # 设置颜色
        bg_color = bg_color or self.default_bg_color
        fg_color = fg_color or self.default_fg_color
        
        # 使用宽度作为字体大小（PIL使用单个尺寸参数）
        font_size = width
        
        # 加载字体
        try:
            font = ImageFont.truetype(font_path, font_size)
        except Exception as e:
            print(f"错误: 无法加载字体文件 {font_path}: {e}")
            return False
        
        # 计算图片尺寸
        font_name = Path(font_path).stem
        title_text = f"字体预览: {font_name} - {width}x{height}"
        
        # 创建临时图像计算文本尺寸
        temp_img = Image.new('RGB', (100, 100), bg_color)
        temp_draw = ImageDraw.Draw(temp_img)
        
        # 计算标题尺寸
        title_bbox = temp_draw.textbbox((0, 0), title_text, font=font)
        title_width = title_bbox[2] - title_bbox[0]
        title_height = title_bbox[3] - title_bbox[1]
        
        # 计算预览文本尺寸
        preview_lines = preview_text.split('\n')
        max_line_width = 0
        total_preview_height = 0
        
        for line in preview_lines:
            line_bbox = temp_draw.textbbox((0, 0), line, font=font)
            line_width = line_bbox[2] - line_bbox[0]
            line_height = line_bbox[3] - line_bbox[1]
            max_line_width = max(max_line_width, line_width)
            total_preview_height += line_height + self.line_spacing
        
        # 计算图片总尺寸
        img_width = max(title_width, max_line_width) + self.margin * 2
        img_height = title_height + total_preview_height + self.margin * 3 + self.line_spacing * 2
        
        # 创建图像
        img = Image.new('RGB', (img_width, img_height), bg_color)
        draw = ImageDraw.Draw(img)
        
        # 绘制标题
        title_x = (img_width - title_width) // 2
        title_y = self.margin
        draw.text((title_x, title_y), title_text, font=font, fill=fg_color)
        
        # 绘制预览文本
        preview_y = title_y + title_height + self.line_spacing * 2
        
        for line in preview_lines:
            line_bbox = draw.textbbox((0, 0), line, font=font)
            line_width = line_bbox[2] - line_bbox[0]
            line_height = line_bbox[3] - line_bbox[1]
            
            line_x = (img_width - line_width) // 2
            draw.text((line_x, preview_y), line, font=font, fill=fg_color)
            preview_y += line_height + self.line_spacing
        
        # 如果显示网格，添加网格线
        if show_grid:
            self._draw_grid(draw, img_width, img_height, font_size)
        
        # 保存或显示图片
        if output_path:
            img.save(output_path, 'PNG')
            print(f"预览图片已保存: {output_path}")
        else:
            # 生成默认文件名
            default_output = f"{font_name}_{width}x{height}_preview.png"
            img.save(default_output, 'PNG')
            print(f"预览图片已保存: {default_output}")
        
        return True
    
    def generate_multi_preview(self, font_path, size_spec, preview_text, output_dir=None,
                              bg_color=None, fg_color=None, show_grid=False):
        """生成多尺寸字体预览图片"""
        
        # 解析尺寸规格
        sizes = self.parse_size_spec(size_spec)
        if not sizes:
            print("错误: 没有有效的尺寸规格")
            return False
        
        # 创建输出目录
        if output_dir:
            os.makedirs(output_dir, exist_ok=True)
        
        font_name = Path(font_path).stem
        print(f"开始生成多尺寸预览: {font_name}")
        print(f"预览尺寸: {', '.join([f'{w}x{h}' for w, h in sizes])}")
        
        success_count = 0
        
        for width, height in sizes:
            # 生成输出文件名
            if output_dir:
                output_file = os.path.join(output_dir, f"{font_name}_{width}x{height}_preview.png")
            else:
                output_file = None
            
            print(f"\n生成尺寸: {width}x{height}")
            
            success = self.generate_single_preview(
                font_path, width, height, preview_text, output_file,
                bg_color=bg_color, fg_color=fg_color, show_grid=show_grid
            )
            
            if success:
                success_count += 1
        
        print(f"\n多尺寸预览完成! 成功生成 {success_count}/{len(sizes)} 个预览图片")
        return success_count > 0
    
    def generate_char_grid(self, font_path, width, height, chars, output_path=None, 
                          grid_cols=16, bg_color=None, fg_color=None):
        """生成字符网格预览"""
        
        # 设置颜色
        bg_color = bg_color or self.default_bg_color
        fg_color = fg_color or self.default_fg_color
        
        # 使用宽度作为字体大小（PIL使用单个尺寸参数）
        font_size = width
        
        # 加载字体
        try:
            font = ImageFont.truetype(font_path, font_size)
        except Exception as e:
            print(f"错误: 无法加载字体文件 {font_path}: {e}")
            return False
        
        # 计算字符尺寸
        temp_img = Image.new('RGB', (100, 100), bg_color)
        temp_draw = ImageDraw.Draw(temp_img)
        
        char_bbox = temp_draw.textbbox((0, 0), '字', font=font)
        char_width = char_bbox[2] - char_bbox[0]
        char_height = char_bbox[3] - char_bbox[1]
        
        # 计算网格尺寸
        cell_width = char_width + 10
        cell_height = char_height + 10
        
        grid_rows = (len(chars) + grid_cols - 1) // grid_cols
        
        img_width = grid_cols * cell_width + self.margin * 2
        img_height = grid_rows * cell_height + self.margin * 2 + 50  # 额外空间用于标题
        
        # 创建图像
        img = Image.new('RGB', (img_width, img_height), bg_color)
        draw = ImageDraw.Draw(img)
        
        # 绘制标题
        font_name = Path(font_path).stem
        title_text = f"字符网格预览: {font_name} - {width}x{height} ({len(chars)}个字符)"
        title_bbox = draw.textbbox((0, 0), title_text, font=font)
        title_width = title_bbox[2] - title_bbox[0]
        title_x = (img_width - title_width) // 2
        draw.text((title_x, self.margin), title_text, font=font, fill=fg_color)
        
        # 绘制字符网格
        grid_y = self.margin + 40
        
        for i, char in enumerate(chars):
            row = i // grid_cols
            col = i % grid_cols
            
            x = col * cell_width + self.margin + (cell_width - char_width) // 2
            y = row * cell_height + grid_y + (cell_height - char_height) // 2
            
            # 绘制字符
            draw.text((x, y), char, font=font, fill=fg_color)
            
            # 绘制网格线
            draw.rectangle([
                col * cell_width + self.margin,
                row * cell_height + grid_y,
                (col + 1) * cell_width + self.margin,
                (row + 1) * cell_height + grid_y
            ], outline=(200, 200, 200), width=1)
            
            # 显示字符编码
            if char != ' ':
                code_text = f"U+{ord(char):04X}"
                code_font = ImageFont.load_default(10)
                code_bbox = draw.textbbox((0, 0), code_text, font=code_font)
                code_width = code_bbox[2] - code_bbox[0]
                code_x = col * cell_width + self.margin + (cell_width - code_width) // 2
                code_y = row * cell_height + grid_y + cell_height - 15
                draw.text((code_x, code_y), code_text, font=code_font, fill=(150, 150, 150))
        
        # 保存图片
        if output_path:
            img.save(output_path, 'PNG')
            print(f"字符网格预览已保存: {output_path}")
        else:
            default_output = f"{font_name}_{width}x{height}_grid.png"
            img.save(default_output, 'PNG')
            print(f"字符网格预览已保存: {default_output}")
        
        return True
    
    def _draw_grid(self, draw, width, height, font_size):
        """绘制网格线"""
        grid_color = (240, 240, 240)
        
        # 水平线
        for y in range(0, height, font_size):
            draw.line([(0, y), (width, y)], fill=grid_color, width=1)
        
        # 垂直线
        for x in range(0, width, font_size):
            draw.line([(x, 0), (x, height)], fill=grid_color, width=1)

def main():
    parser = argparse.ArgumentParser(description='字体预览生成器')
    parser.add_argument('font_path', help='字体文件路径')
    parser.add_argument('size_spec', help='字体大小规格，支持格式: 16, 16x16, 12x13,13x13,14x14')
    parser.add_argument('preview_text', help='预览文本(支持多行，使用\\n分隔)')
    parser.add_argument('-o', '--output', help='输出图片路径或目录')
    parser.add_argument('--bg-color', help='背景颜色，格式: R,G,B (默认: 255,255,255)')
    parser.add_argument('--fg-color', help='前景颜色，格式: R,G,B (默认: 0,0,0)')
    parser.add_argument('--grid', action='store_true', help='显示网格')
    parser.add_argument('--char-grid', help='生成字符网格预览，指定字符集文件')
    parser.add_argument('--multi', action='store_true', help='多尺寸预览模式，输出为目录')
    
    args = parser.parse_args()
    
    # 处理颜色参数
    bg_color = None
    fg_color = None
    
    if args.bg_color:
        try:
            bg_color = tuple(map(int, args.bg_color.split(',')))
        except ValueError:
            print("错误: 背景颜色格式不正确，使用默认颜色")
    
    if args.fg_color:
        try:
            fg_color = tuple(map(int, args.fg_color.split(',')))
        except ValueError:
            print("错误: 前景颜色格式不正确，使用默认颜色")
    
    # 创建预览生成器
    generator = FontPreviewGenerator()
    
    # 处理预览文本中的换行符
    preview_text = args.preview_text.replace('\\n', '\n')
    
    # 解析尺寸规格
    sizes = generator.parse_size_spec(args.size_spec)
    if not sizes:
        print("错误: 没有有效的尺寸规格")
        sys.exit(1)
    
    # 生成预览
    if args.char_grid:
        # 生成字符网格预览
        if not os.path.exists(args.char_grid):
            print(f"错误: 字符集文件 {args.char_grid} 不存在")
            return
        
        # 解析字符集文件
        chars = []
        with open(args.char_grid, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    chars.extend(list(line))
        
        if not chars:
            print("错误: 字符集文件中没有有效字符")
            return
        
        # 字符网格预览只支持单尺寸
        if len(sizes) > 1:
            print("警告: 字符网格预览只支持单尺寸，使用第一个尺寸")
        
        width, height = sizes[0]
        success = generator.generate_char_grid(
            args.font_path, width, height, chars, args.output,
            bg_color=bg_color, fg_color=fg_color
        )
    else:
        # 生成文本预览
        if len(sizes) > 1 or args.multi:
            # 多尺寸预览模式
            success = generator.generate_multi_preview(
                args.font_path, args.size_spec, preview_text, args.output,
                bg_color=bg_color, fg_color=fg_color, show_grid=args.grid
            )
        else:
            # 单尺寸预览模式
            width, height = sizes[0]
            success = generator.generate_single_preview(
                args.font_path, width, height, preview_text, args.output,
                bg_color=bg_color, fg_color=fg_color, show_grid=args.grid
            )
    
    if not success:
        sys.exit(1)

if __name__ == '__main__':
    main()