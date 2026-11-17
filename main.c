/*
 * Project 44: Smart Attendance System (Face-ID Simulation)
 *
 * Features
 *  - Student records (roll_no, name, face_id) stored in structures
 *  - Dynamic student list using pointers, malloc() and realloc()
 *  - Reads attendance log (Face IDs) from a text file, matches them to
 *    students and marks presence
 *  - Calculates attendance percentage and lists defaulters (< 75%)
 *
 * Students file format (one per line, CSV):
 *      roll_no,name,face_id
 *      101,Aman Sharma,5001
 *
 * Attendance log format (one line = one class session, space separated
 * Face IDs of everyone detected in that session; '#' starts a comment):
 *      5001 5002 5004
 *      5001 5003
 *
 * Compile:  gcc -Wall -Wextra -o attendance attendance.c
 * Run:      ./attendance
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_LEN          50
#define LINE_LEN          1024
#define INITIAL_CAPACITY  4
#define DEFAULT_THRESHOLD 75.0f

/* ---------- Data structures ---------- */

typedef struct {
    int  roll_no;
    char name[NAME_LEN];
    int  face_id;
    int  present_days;      /* number of sessions attended */
} Student;

typedef struct {
    Student *list;          /* dynamically allocated array of students */
    int      count;         /* students currently stored               */
    int      capacity;      /* allocated slots                         */
    int      total_sessions;/* class sessions processed from logs     */
} Manager;

/* ---------- Dynamic list management (CO4) ---------- */

int init_manager(Manager *m)
{
    m->list = (Student *)malloc(INITIAL_CAPACITY * sizeof(Student));
    if (m->list == NULL) {
        fprintf(stderr, "Error: memory allocation failed.\n");
        return 0;
    }
    m->count = 0;
    m->capacity = INITIAL_CAPACITY;
    m->total_sessions = 0;
    return 1;
}

void free_manager(Manager *m)
{
    free(m->list);
    m->list = NULL;
    m->count = m->capacity = m->total_sessions = 0;
}

/* Doubles the capacity when the array is full. */
int ensure_capacity(Manager *m)
{
    if (m->count < m->capacity)
        return 1;

    int new_cap = m->capacity * 2;
    Student *tmp = (Student *)realloc(m->list, new_cap * sizeof(Student));
    if (tmp == NULL) {
        fprintf(stderr, "Error: could not expand student list.\n");
        return 0;
    }
    m->list = tmp;
    m->capacity = new_cap;
    return 1;
}

/* ---------- Lookup helpers ---------- */

Student *find_by_roll(const Manager *m, int roll_no)
{
    for (int i = 0; i < m->count; i++)
        if (m->list[i].roll_no == roll_no)
            return &m->list[i];
    return NULL;
}

Student *find_by_face(const Manager *m, int face_id)
{
    for (int i = 0; i < m->count; i++)
        if (m->list[i].face_id == face_id)
            return &m->list[i];
    return NULL;
}

/* ---------- Student management with validation (CO2) ---------- */

/* Return codes: 0 = added, 1 = invalid data, 2 = duplicate, 3 = memory error */
int add_student(Manager *m, int roll_no, const char *name, int face_id)
{
    if (roll_no <= 0 || face_id <= 0 || name == NULL || name[0] == '\0')
        return 1;
    if (find_by_roll(m, roll_no) != NULL || find_by_face(m, face_id) != NULL)
        return 2;
    if (!ensure_capacity(m))
        return 3;

    Student *s = &m->list[m->count++];
    s->roll_no = roll_no;
    strncpy(s->name, name, NAME_LEN - 1);
    s->name[NAME_LEN - 1] = '\0';
    s->face_id = face_id;
    s->present_days = 0;
    return 0;
}

/* Reads students from a CSV text file. Returns number of students added. */
int load_students_from_file(Manager *m, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error: cannot open '%s'.\n", filename);
        return 0;
    }

    char line[LINE_LEN];
    int added = 0, line_no = 0;

    while (fgets(line, sizeof line, fp) != NULL) {
        line_no++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        int roll, face;
        char name[NAME_LEN];
        if (sscanf(line, "%d,%49[^,],%d", &roll, name, &face) != 3) {
            printf("  Line %d skipped: bad format.\n", line_no);
            continue;
        }
        int rc = add_student(m, roll, name, face);
        if (rc == 0)       added++;
        else if (rc == 1)  printf("  Line %d skipped: invalid data.\n", line_no);
        else if (rc == 2)  printf("  Line %d skipped: duplicate roll no / face id.\n", line_no);
        else               break;
    }
    fclose(fp);
    return added;
}

/* ---------- Attendance marking (CO1, CO2) ---------- */

/* Marks a student present in the current session (once only). */
int mark_present(Student *s, int *already_marked)
{
    if (*already_marked)
        return 0;
    s->present_days++;
    *already_marked = 1;
    return 1;
}

/*
 * Reads the attendance log. Each non-empty line is one session containing
 * Face IDs. Unknown or invalid IDs are reported; duplicates in a single
 * session are counted once.
 */
int process_attendance_log(Manager *m, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error: cannot open '%s'.\n", filename);
        return 0;
    }
    if (m->count == 0) {
        printf("No students loaded. Add students first.\n");
        fclose(fp);
        return 0;
    }

    /* per-session "already marked" flags, allocated dynamically */
    int *marked = (int *)malloc(m->count * sizeof(int));
    if (marked == NULL) {
        fprintf(stderr, "Error: memory allocation failed.\n");
        fclose(fp);
        return 0;
    }

    char line[LINE_LEN];
    int sessions = 0, line_no = 0;

    while (fgets(line, sizeof line, fp) != NULL) {
        line_no++;
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0')
            continue;

        sessions++;
        memset(marked, 0, m->count * sizeof(int));
        printf("Session %d:\n", m->total_sessions + sessions);

        char *tok = strtok(line, " \t\r\n,");
        while (tok != NULL) {
            char *end;
            long id = strtol(tok, &end, 10);

            if (*end != '\0' || id <= 0) {
                printf("  Line %d: invalid Face ID '%s' ignored.\n", line_no, tok);
            } else {
                Student *s = find_by_face(m, (int)id);
                if (s == NULL) {
                    printf("  Face ID %ld not recognised (no matching student).\n", id);
                } else {
                    int idx = (int)(s - m->list);   /* pointer arithmetic */
                    if (mark_present(s, &marked[idx]))
                        printf("  Present: %d - %s\n", s->roll_no, s->name);
                    else
                        printf("  Duplicate scan for %s ignored.\n", s->name);
                }
            }
            tok = strtok(NULL, " \t\r\n,");
        }
    }

    free(marked);
    fclose(fp);
    m->total_sessions += sessions;
    return sessions;
}

/* ---------- Calculations & reports (CO3) ---------- */

float attendance_percentage(const Student *s, int total_sessions)
{
    if (total_sessions <= 0)
        return 0.0f;
    return (s->present_days * 100.0f) / total_sessions;
}

void display_students(const Manager *m)
{
    if (m->count == 0) {
        printf("No students stored.\n");
        return;
    }
    printf("\n%-8s %-25s %-8s\n", "Roll No", "Name", "Face ID");
    printf("-------------------------------------------\n");
    for (int i = 0; i < m->count; i++)
        printf("%-8d %-25s %-8d\n", m->list[i].roll_no, m->list[i].name, m->list[i].face_id);
}

void print_report(const Manager *m)
{
    if (m->count == 0 || m->total_sessions == 0) {
        printf("Nothing to report yet (load students and process a log first).\n");
        return;
    }
    printf("\nAttendance Report (%d session%s)\n", m->total_sessions,
           m->total_sessions == 1 ? "" : "s");
    printf("%-8s %-25s %-8s %-10s %s\n", "Roll No", "Name", "Present", "Percent", "Status");
    printf("---------------------------------------------------------------\n");
    for (int i = 0; i < m->count; i++) {
        float pct = attendance_percentage(&m->list[i], m->total_sessions);
        char pbuf[16];
        snprintf(pbuf, sizeof pbuf, "%.2f%%", pct);
        printf("%-8d %-25s %-8d %-10s %s\n",
               m->list[i].roll_no, m->list[i].name, m->list[i].present_days, pbuf,
               pct < DEFAULT_THRESHOLD ? "DEFAULTER" : "OK");
    }
}

void print_defaulters(const Manager *m)
{
    if (m->total_sessions == 0) {
        printf("No attendance processed yet.\n");
        return;
    }
    int found = 0;
    printf("\nDefaulters (attendance below %.0f%%)\n", DEFAULT_THRESHOLD);
    printf("%-8s %-25s %-10s\n", "Roll No", "Name", "Percent");
    printf("-------------------------------------------\n");
    for (int i = 0; i < m->count; i++) {
        float pct = attendance_percentage(&m->list[i], m->total_sessions);
        if (pct < DEFAULT_THRESHOLD) {
            printf("%-8d %-25s %.2f%%\n", m->list[i].roll_no, m->list[i].name, pct);
            found++;
        }
    }
    if (!found)
        printf("None. Everyone meets the %.0f%% requirement.\n", DEFAULT_THRESHOLD);
}

/* ---------- Input helpers ---------- */

void read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    if (fgets(buf, (int)size, stdin) == NULL)
        buf[0] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';
}

/* Reads a positive integer; returns -1 on invalid input. */
int read_int(const char *prompt)
{
    char buf[64], *end;
    read_line(prompt, buf, sizeof buf);
    long v = strtol(buf, &end, 10);
    if (buf[0] == '\0' || *end != '\0' || v < 0 || v > 1000000000L)
        return -1;
    return (int)v;
}

void add_student_interactive(Manager *m)
{
    char name[NAME_LEN];
    int roll = read_int("Roll number : ");
    read_line("Name        : ", name, sizeof name);
    int face = read_int("Face ID     : ");

    switch (add_student(m, roll, name, face)) {
        case 0:  printf("Student added.\n"); break;
        case 1:  printf("Invalid input. Roll no and Face ID must be positive numbers.\n"); break;
        case 2:  printf("Roll number or Face ID already exists.\n"); break;
        default: printf("Memory error.\n");
    }
}

void print_menu(void)
{
    printf("\n===== Smart Attendance System (Face-ID Simulation) =====\n");
    printf("1. Add student manually\n");
    printf("2. Load students from file\n");
    printf("3. Process attendance log file\n");
    printf("4. Display all students\n");
    printf("5. Show attendance report\n");
    printf("6. Show defaulters (< 75%%)\n");
    printf("7. Exit\n");
}

/* ---------- main ---------- */

int main(void)
{
    Manager mgr;
    if (!init_manager(&mgr))
        return 1;

    char fname[128];
    int choice;

    do {
        print_menu();
        choice = read_int("Enter choice: ");

        switch (choice) {
            case 1:
                add_student_interactive(&mgr);
                break;
            case 2:
                read_line("Students file name: ", fname, sizeof fname);
                printf("%d student(s) loaded.\n", load_students_from_file(&mgr, fname));
                break;
            case 3:
                read_line("Attendance log file name: ", fname, sizeof fname);
                printf("%d session(s) processed.\n", process_attendance_log(&mgr, fname));
                break;
            case 4:
                display_students(&mgr);
                break;
            case 5:
                print_report(&mgr);
                break;
            case 6:
                print_defaulters(&mgr);
                break;
            case 7:
                printf("Goodbye!\n");
                break;
            default:
                printf("Invalid choice. Try again.\n");
        }
    } while (choice != 7);

    free_manager(&mgr);
    return 0;
}
