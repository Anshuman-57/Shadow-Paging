#!/usr/bin/env python3
"""
Memory Debugger Log Visualizer - Shadow Paging Analysis
"""

import re, os, sys
from collections import defaultdict
import matplotlib.pyplot as plt
import numpy as np

class LogParser:
    def __init__(self, log_file):
        self.log_file = log_file
        self.writes = []
        self.start_time = self.end_time = None
        
    def parse(self):
        try:
            with open(self.log_file, 'r') as f:
                lines = f.readlines()
        except FileNotFoundError:
            print(f"❌ Log file '{self.log_file}' not found")
            return False
        
        pattern = r'\|\s*(\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}:\d{2})\s*\|\s*([\d.]+)\s*\|\s*(0x[0-9a-f]+)\s*\|\s*(\d+)\s*\|\s*(-?\d+)\s*\|\s*(-?\d+)\s*\|\s*(\d+)\s*\|'
        
        for line in lines:
            match = re.search(pattern, line)
            if match:
                ts, t_sec, addr, idx, old, new, cnt = match.groups()
                write = {
                    'timestamp': ts, 'time_sec': float(t_sec), 'address': addr,
                    'index': int(idx), 'old_value': int(old), 'new_value': int(new),
                    'count': int(cnt), 'delta': int(new) - int(old)
                }
                self.writes.append(write)
                if self.start_time is None:
                    self.start_time = write['time_sec']
                self.end_time = write['time_sec']
        
        return len(self.writes) > 0
    
    def get_stats(self):
        if not self.writes:
            return None
        
        stats = {
            'total_writes': len(self.writes),
            'unique_indices': len(set(w['index'] for w in self.writes)),
            'duration': self.end_time - self.start_time,
            'writes_per_index': defaultdict(int),
            'memory_changes': defaultdict(list),
        }
        
        for w in self.writes:
            idx = w['index']
            stats['writes_per_index'][idx] += 1
            stats['memory_changes'][idx].append({
                'time': w['time_sec'] - self.start_time,
                'old': w['old_value'], 'new': w['new_value'], 'delta': w['delta']
            })
        
        return stats


class Visualizer:
    def __init__(self, parser, out_dir='visualizations'):
        self.parser = parser
        self.stats = parser.get_stats()
        self.out_dir = out_dir
        os.makedirs(out_dir, exist_ok=True)
        print(f"✓ Output: {out_dir}/")
    
    def save(self, name):
        plt.tight_layout()
        path = f"{self.out_dir}/{name}"
        plt.savefig(path, dpi=300, bbox_inches='tight')
        print(f"  ✓ {name}")
        plt.close()
    
    def plot_timeline(self):
        fig, ax = plt.subplots(figsize=(14, 8))
        indices = sorted(self.stats['writes_per_index'].keys())
        colors = plt.cm.tab20(np.linspace(0, 1, len(indices)))
        
        for idx, color in zip(indices, colors):
            changes = self.stats['memory_changes'][idx]
            times = [c['time'] for c in changes]
            old_vals = [c['old'] for c in changes]
            new_vals = [c['new'] for c in changes]
            
            ax.hlines(idx, 0, self.stats['duration'], colors='lightgray', linestyle='--')
            for t, o, n in zip(times, old_vals, new_vals):
                ax.annotate('', xy=(t, n), xytext=(t, o),
                           arrowprops=dict(arrowstyle='<->', color=color, lw=2))
                ax.plot(t, n, 'o', color=color, markersize=8)
        
        ax.set_ylabel('Memory Index', fontweight='bold')
        ax.set_xlabel('Time (s)', fontweight='bold')
        ax.set_title('Memory Write Timeline', fontweight='bold', fontsize=12)
        ax.grid(True, alpha=0.3)
        self.save('01_timeline.png')
    
    def plot_heatmap(self):
        fig, ax = plt.subplots(figsize=(12, 6))
        indices = sorted(self.stats['writes_per_index'].keys())
        counts = [self.stats['writes_per_index'][i] for i in indices]
        colors = ['#ff6b6b' if c > 3 else '#4ecdc4' if c > 1 else '#95e1d3' for c in counts]
        
        ax.barh(indices, counts, color=colors, edgecolor='black', linewidth=1.5)
        for i, (idx, c) in enumerate(zip(indices, counts)):
            ax.text(c + 0.1, idx, str(c), va='center', fontweight='bold')
        
        ax.set_xlabel('Write Count', fontweight='bold')
        ax.set_ylabel('Memory Index', fontweight='bold')
        ax.set_title('Memory Access Heatmap', fontweight='bold', fontsize=12)
        ax.grid(True, alpha=0.3, axis='x')
        self.save('02_heatmap.png')
    
    def plot_evolution(self):
        indices = sorted(self.stats['writes_per_index'].keys())
        cols, rows = 3, (len(indices) + 2) // 3
        fig, axes = plt.subplots(rows, cols, figsize=(15, 3*rows))
        axes = axes.flatten() if len(indices) > 1 else [axes]
        colors = plt.cm.rainbow(np.linspace(0, 1, len(indices)))
        
        for ax, idx, color in zip(axes, indices, colors):
            changes = self.stats['memory_changes'][idx]
            times = [0] + [c['time'] for c in changes]
            values = [changes[0]['old']] + [c['new'] for c in changes]
            
            ax.plot(times, values, 'o-', color=color, linewidth=2, markersize=8)
            ax.fill_between(times, values, alpha=0.3, color=color)
            ax.set_title(f'Index {idx}', fontweight='bold', fontsize=10)
            ax.grid(True, alpha=0.3)
        
        for i in range(len(indices), len(axes)):
            axes[i].set_visible(False)
        
        fig.suptitle('Value Evolution Timeline', fontsize=14, fontweight='bold')
        self.save('03_evolution.png')
    
    def plot_shadow_diff(self):
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
        indices = sorted(self.stats['writes_per_index'].keys())
        
        initial = [self.stats['memory_changes'][i][0]['old'] for i in indices]
        final = [self.stats['memory_changes'][i][-1]['new'] for i in indices]
        
        ax1.barh(indices, initial, color='#95e1d3', edgecolor='black', linewidth=1.5)
        ax1.set_title('BEFORE (Initial)', fontweight='bold')
        ax1.set_xlabel('Value')
        
        ax2.barh(indices, final, color='#ff6b6b', edgecolor='black', linewidth=1.5)
        ax2.set_title('AFTER (Final)', fontweight='bold')
        ax2.set_xlabel('Value')
        
        fig.suptitle('Shadow Page: Target vs Mirror Memory', fontweight='bold', fontsize=12)
        self.save('04_shadow_diff.png')
    
    def generate_html(self):
        indices = sorted(self.stats['writes_per_index'].keys())
        rows = ''.join([
            f"<tr><td>{i}</td><td>{self.stats['writes_per_index'][i]}</td>"
            f"<td>{self.stats['memory_changes'][i][0]['old']}</td>"
            f"<td>{self.stats['memory_changes'][i][-1]['new']}</td>"
            f"<td>{sum(c['delta'] for c in self.stats['memory_changes'][i]):+d}</td></tr>"
            for i in indices
        ])
        
        html = f"""<!DOCTYPE html>
<html><head><meta charset="UTF-8"><title>Memory Debugger Report</title>
<style>
body {{font-family: Arial; background: linear-gradient(135deg, #667eea, #764ba2); padding: 20px; margin: 0;}}
.container {{max-width: 1200px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 10px 40px rgba(0,0,0,0.3);}}
header {{background: linear-gradient(135deg, #667eea, #764ba2); color: white; padding: 30px; text-align: center;}}
header h1 {{margin: 0; font-size: 2.5em;}}
.content {{padding: 40px;}}
.stats {{display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 30px 0;}}
.stat {{background: linear-gradient(135deg, #667eea, #764ba2); color: white; padding: 20px; border-radius: 8px; text-align: center;}}
.stat h3 {{font-size: 2em; margin: 0;}}
.viz {{margin: 30px 0; border: 1px solid #eee; border-radius: 8px; overflow: hidden;}}
.viz-title {{background: #f5f5f5; padding: 15px; border-bottom: 2px solid #667eea; font-weight: bold;}}
img {{width: 100%; display: block;}}
table {{width: 100%; border-collapse: collapse; margin-top: 15px;}}
th, td {{padding: 12px; text-align: left; border-bottom: 1px solid #ddd;}}
th {{background: #667eea; color: white;}}
</style></head><body>
<div class="container">
<header><h1>🔍 Memory Debugger Report</h1><p>Shadow Paging Analysis</p></header>
<div class="content">
<div class="stats">
<div class="stat"><h3>{self.stats['total_writes']}</h3><p>Total Writes</p></div>
<div class="stat"><h3>{self.stats['unique_indices']}</h3><p>Unique Indices</p></div>
<div class="stat"><h3>{self.stats['duration']:.3f}s</h3><p>Duration</p></div>
</div>
<div class="viz"><div class="viz-title">📈 Timeline</div><img src="01_timeline.png"></div>
<div class="viz"><div class="viz-title">🔥 Heatmap</div><img src="02_heatmap.png"></div>
<div class="viz"><div class="viz-title">📉 Evolution</div><img src="03_evolution.png"></div>
<div class="viz"><div class="viz-title">🔄 Shadow Diff</div><img src="04_shadow_diff.png"></div>
<h3>📋 Memory Details</h3>
<table><tr><th>Index</th><th>Writes</th><th>Initial</th><th>Final</th><th>Change</th></tr>{rows}</table>
</div></div></body></html>"""
        
        with open(f"{self.out_dir}/index.html", 'w') as f:
            f.write(html)
        print(f"  ✓ index.html")
    
    def print_summary(self):
        print("\n" + "="*70)
        print("MEMORY DEBUGGER - SHADOW PAGING ANALYSIS")
        print("="*70)
        print(f"\n📊 Stats: {self.stats['total_writes']} writes | {self.stats['unique_indices']} indices | {self.stats['duration']:.3f}s")
        print("\n📈 Writes per Index:")
        for idx in sorted(self.stats['writes_per_index'].keys()):
            c = self.stats['writes_per_index'][idx]
            print(f"  Index[{idx:2d}]: {'█'*c}{'░'*(10-min(c,10))} {c}")
        print("\n💾 Memory Changes:")
        for idx in sorted(self.stats['memory_changes'].keys()):
            ch = self.stats['memory_changes'][idx]
            init, fin = ch[0]['old'], ch[-1]['new']
            delta = sum(c['delta'] for c in ch)
            print(f"  Index[{idx:2d}]: {init:6d} → {fin:6d} (Δ {delta:+6d})")
        print("\n" + "="*70 + "\n")

def main():
    log_file = sys.argv[1] if len(sys.argv) > 1 else "logs/memory_diff.log"
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "visualizations"
    
    print(f"📖 Parsing: {log_file}")
    parser = LogParser(log_file)
    
    if not parser.parse():
        print("❌ No log entries found")
        sys.exit(1)
    
    print(f"✓ Parsed {len(parser.writes)} events\n")
    
    viz = Visualizer(parser, out_dir)
    viz.print_summary()
    
    print("🎨 Generating visualizations...")
    viz.plot_timeline()
    viz.plot_heatmap()
    viz.plot_evolution()
    viz.plot_shadow_diff()
    viz.generate_html()
    
    print(f"\n✅ Done! Open {out_dir}/index.html in browser\n")

if __name__ == "__main__":
    main()