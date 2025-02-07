def try_heatmap(name, title, x_label, y_label, x_ticks, y_ticks, data):
    if True:
        import matplotlib.pyplot as plt
        plt.rc('font', size=6) 
        
        fig, ax = plt.subplots(figsize=(len(x_ticks) / 3 + 4, len(y_ticks) / 3 + 4))
        im = ax.imshow(data, interpolation="nearest")
        
        ax.set_xticks(list(range(len(x_ticks))))
        ax.set_xticklabels(list(map(str, x_ticks)))
        ax.set_yticks(list(range(len(y_ticks))))
        ax.set_yticklabels(list(map(str, y_ticks)))
        
        plt.setp(ax.get_xticklabels(), rotation=45, ha="right", rotation_mode="anchor")
        
        for i in range(len(y_ticks)):
            for j in range(len(x_ticks)):
                text = ax.text(j, i, data[i][j], ha="center", va="center", color="w")
        
        ax.set_title(title)
        ax.set_xlabel(x_label)
        ax.set_ylabel(y_label)
        # fig.tight_layout()
        
        plt.savefig(f"{name}.svg") 
        plt.close(fig)
    #except:
    #    print("failed to plot")
